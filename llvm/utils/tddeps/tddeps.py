#!/usr/bin/env python
# -*-coding:utf-8-*-

import hashlib
import optparse
import os
import os.path
import re
import stat
import subprocess
import sys

re_inc = re.compile(r'^\s*#?\s*include\s*"([^"]+)"')
re_header_suffixes = re.compile(r'.*\.(def|inc|h|td)$')
re_header_tmpl_suffixes = re.compile(r'(?P<basename>.+\.(def|h|inc))\.(in|cmake)$')
re_source_suffixes = re.compile(r'.*\.(c|cc|cpp)$')
re_header_source_suffixes = re.compile(r'.*\.(def|inc|h|td|c|cc|cpp)$')

parent_scope = "PARENT_SCOPE"

out = sys.stdout

args = sys.argv[1:]
action = None
try:
    i = args.index("--action")
    action = args[i+1:]
    args = args[:i]
except ValueError:
    pass

parser = optparse.OptionParser()
parser.add_option("-i", dest="infile",
                  help="in{.tmp,.stamp}")
parser.add_option("-o", dest="out",
                  help="out")
parser.add_option("-s", dest="stampdir",
                  help="stampdir")
parser.add_option("-v", dest="verbose", action="store_true",
                  help="Verbose")
(opts, args) = parser.parse_args(args)

outfile = None
if opts.out:
    outfile = opts.out

infile_mtime = 0
if opts.infile:
    outfile = opts.infile+".tmp"
    st = os.lstat(opts.infile)
    #infile_mtime = st.st_mtime

if outfile:
    out = open(outfile, 'w')

users = []
dir_names = {}

if opts.stampdir:
    assert os.path.isdir(opts.stampdir)
    dir_names[opts.stampdir] = "STAMPDIR"

re_dir_def = re.compile(r'^((?P<name>\w+)=)?(?P<dir>.+)$')
for a in args:
    m = re_dir_def.match(a)
    assert m
    dir = os.path.normpath(m.group("dir"))
    assert os.path.isdir(dir)
    if m.group("name"):
        dir_names[dir] = m.group("name")
    if not os.path.isdir(dir):
        break
    users.append(dir)

re_dirnames_dir = re.compile(r'^(?P<dir>' + '|'.join(dir_names.keys()) + ')(?P<suf>(/.*)?)$')
def with_dir_names(f):
    m = re_dirnames_dir.match(f)
    if m:
        return "${%s}%s" % (dir_names[m.group("dir")], m.group("suf"))
    return f

assert(users)

re_excludes = re.compile(
    "^(|" + '|'.join(
        ["\..*|.*~.*|Makefile.*"]
        + ["autoconf|docs|images|test|tests"]
        + map(lambda x: ".*\." + x, [
                "cmake", "html", "ml", "mli", "pl", "py", "pyc", "sh", "TXT", "txt",
                ])
        )
    + ")$")

deps = {}

# FIXME: Clang standalone build?
includes = [
    os.path.normpath(os.path.join(users[0], "include")),
    os.path.normpath(os.path.join(users[0], "utils/unittest/googletest/include")),
    ]

if len(users)>=2:
    includes.append(os.path.normpath(os.path.join(users[1], "include")))

include_files = {}
all_files = []
all_srcs = set()
all_mtimes = {}
cmake_targets = {}

def walk(dirs, include_dirs):
    subdirs = dirs[:]
    while subdirs:
        path = subdirs.pop()

        include_prefix = None
        for incdir in include_dirs:
            d = os.path.relpath(path, incdir)
            if not d.startswith('..'):
                include_prefix = d
                break

        files = os.listdir(path)

        cmake_found = ("CMakeLists.txt" in files)
        srcs = []

        for file in files:
            if re_excludes.match(file):
                continue

            filepath = os.path.join(path, file)

            mt = re_header_tmpl_suffixes.match(file)
            if mt:
                if include_prefix:
                    # Add generated files as "independent"
                    include_files[os.path.join(include_prefix, mt.group("basename"))] = filepath
                    deps[filepath] = frozenset()
                continue

            if re_source_suffixes.match(file):
                if cmake_found:
                    st = os.lstat(filepath)
                    if st.st_mtime > infile_mtime:
                        srcs.append(filepath)
                    all_mtimes[filepath] = st.st_mtime
                continue

            st = os.lstat(filepath)
            if stat.S_ISDIR(st.st_mode):
                subdirs.append(filepath)
                continue

            m = re_header_suffixes.match(file)
            if m:
                if st.st_mtime > infile_mtime:
                    all_files.append(filepath)
                all_mtimes[filepath] = st.st_mtime
                if m.group(1) == "td":
                    all_srcs.add(filepath)
                if include_prefix:
                    include_files[os.path.join(include_prefix, file)] = filepath
                continue

        if srcs:
            cmake_targets[path] = frozenset(srcs)
            all_files.extend(srcs)
            all_srcs.update(srcs)
            yield os.path.join(path, "CMakeLists.txt")

def em(cwd, args):
    # for i in args:
    #     i = os.path.relpath(i, cwd)
    #     assert(not i.startswith('.'))
    return (cwd, map(lambda f: os.path.relpath(f, cwd), args))

def xargs(dirwalker, arg_max=32767):
    cwd = None
    args = None
    args_len = None
    try:
        while True:
            #(path,file) = dirwalker.next()
            filepath = dirwalker.next()
            (path,file) = os.path.split(filepath)
            #print "LOOP    <%s/%s>" % (path,file)
            if cwd and cwd != path:
                old_cwd = cwd
                cwd = os.path.normpath(os.path.commonprefix([cwd,path]))
                # FIXME: Consider root. (Should be rejected?)
                #print "oldcwd\t=%s" % old_cwd
                #print "path\t=%s (%s)" % (path,file)
                #print "newcwd\t=%s" % cwd
                if os.path.relpath(path, cwd).startswith(".."):
                    cwd = os.path.dirname(cwd)
                    #print "newcwd\t=%s (again)" % cwd
                if cwd != old_cwd:
                    rel_old_cwd = os.path.relpath(old_cwd, cwd)
                    if rel_old_cwd.startswith(".."):
                        # The case when:
                        #   old=root/clang-tools-extra/foo
                        #   new=root/clang/bar
                        # Then, cwd=root/clang. It should be cwd=root.
                        #print "%s -- %s -- %s XXX" % (cwd,old_cwd,rel_old_cwd)
                        cwd = os.path.dirname(cwd)
                        rel_old_cwd = os.path.relpath(old_cwd, cwd)
                        assert(not rel_old_cwd.startswith(".."))
                    #print "%s -- %s(%d)" % (newcwd,rel_old_cwd,len(rel_old_cwd))
                    lendelta = len(args) * (1 + len(rel_old_cwd))
                    #print "cwd=<%s> n=%d len=%d(%d) delta=%d" % (cwd, len(args), args_len, 1+len(' '.join(map(lambda f:os.path.relpath(f,cwd),args))), lendelta)
                    #assert(args_len == 1+len(' '.join(map(lambda f:os.path.relpath(f,cwd),args))))
                    assert(args_len < arg_max)
                    args_len += lendelta

                    if args_len >= arg_max:
                        yield em(old_cwd, args)
                        cwd = None

            if not cwd:
                cwd = path
                args = []
                args_len = 0

            #filepath = os.path.join(path, file)
            #print "FILEPATH<%s>" % filepath
            #print "cwd=%s f=%s" % (cwd, filepath)

            assert(args_len < arg_max)
            args_len += 1 + len(os.path.relpath(filepath, cwd))
            if args_len >= arg_max:
                yield em(cwd, args)
                cwd = path
                args = []
                args_len = 1 + len(file)

            #print "APPEND<%s>" % filepath
            assert not os.path.relpath(filepath, cwd).startswith('.')
            #print "cwd=<%s> n=%d len=%d(%d)" % (cwd, len(args), args_len, 1+len(' '.join(map(lambda f:os.path.relpath(f,cwd),args))))
            args.append(filepath)
            #print "cwd=<%s> n=%d len=%d(%d)" % (cwd, len(args), args_len, 1+len(' '.join(map(lambda f:os.path.relpath(f,cwd),args))))
            #assert(args_len == 1+len(' '.join(map(lambda f:os.path.relpath(f,cwd),args))))
    except StopIteration:
        #assert(args_len == 1+len(' '.join(map(lambda f:os.path.relpath(f,cwd),args))))
        if args:
            yield em(cwd, args)

def get_name(ident):
    return ident.replace('.', '_').replace('-','_')

def get_group_name(names):
    if not names:
        return 'None'
    a = []
    for dir in sorted(names):
        stem,suf = os.path.splitext(os.path.basename(dir))
        a.append(stem)
    return '_'.join(a).replace('.', '_')

def get_group_name_hashed(names):
    name = get_group_name(names)
    if len(name)>40:
        name = hashlib.sha1(name).hexdigest()
    return name

################################

def xgrep_subprocess(dirwalker, grep_Expr):
    for cwd,args in xargs(dirwalker):
        grep = subprocess.Popen(
            executable="grep",
            args=[
                "grep", "-n",
                "-Ee", grep_Expr,
                ] + args,
            stdout=subprocess.PIPE,
            cwd=cwd
            )
        yield (cwd,grep,args)

def xgrep_process(grep_tuple):
    re_grep_n_include = re.compile(r'^(?P<filepath>.+):\d+:\s*#?\s*include\s*"(?P<includee>[^"]+)')
    cwd,grep,args = grep_tuple
    filepath = None
    includees = None
    args_left = set(args)
    for line in grep.stdout:
        m = re_grep_n_include.match(line)
        if not m:
            continue
        f = m.group("filepath")
        args_left.discard(f)
        f = os.path.join(cwd, f)
        if not filepath:
            filepath = f
            includees = []
        if filepath != f:
            yield (filepath, frozenset(includees))
            filepath = f
            includees = []
        includees.append(m.group("includee"))

    if includees:
        yield (filepath, frozenset(includees))

    # Report "NOT HIT" files.
    for f in args_left:
        yield (os.path.join(cwd, f), frozenset())

    r = grep.wait()
    assert r == 0

def xgrep(dirwalker, njobs=8):
    jobs = []
    xgrep_gen = xgrep_subprocess(dirwalker, r'^\s*#?\s*include\s*"')
    try:
        while True:
            if len(jobs) < njobs:
                jobs.append(xgrep_gen.next())
                continue
            assert len(jobs) > 0
            for fn_tuple in xgrep_process(jobs.pop(0)):
                yield fn_tuple
    except StopIteration:
        for grep_job in jobs:
            for fn_tuple in xgrep_process(grep_job):
                yield fn_tuple

excluded_dir = set()

def xgrep_cmake_process(grep_tuple):
    re_cmake_target_cmd = re.compile(r'(?P<filepath>.+):\d+:\s*(?P<cmd>add_(tablegen|(llvm|clang|extra)_(example|executable|library|loadable_module|target|tool|unittest|utility)))\s*\(\s*(?P<name>\w[-\w]*)')
    re_exclude = re.compile(r'^(?P<filepath>.+):\d+:.*#\s*TDDEPS:\s*exclude')
    cwd,grep,args = grep_tuple
    filepath = None
    for line in grep.stdout:
        m = re_exclude.search(line)
        if m:
            excluded_dir.add(os.path.dirname(os.path.join(cwd, m.group("filepath"))))
            continue
        m = re_cmake_target_cmd.match(line)
        if not m:
            continue
        name = m.group("name").replace('-','_')
        if m.group("cmd") == "add_llvm_target":
            name = "LLVM" + name
        assert not filepath == m.group("filepath")
        filepath = m.group("filepath")
        yield (os.path.join(cwd, filepath), name)

    r = grep.wait()
    assert r == 0

def xgrep_cmake(dirwalker, njobs=8):
    jobs = []
    xgrep_gen = xgrep_subprocess(dirwalker, r'^\s*add_(tablegen|(llvm|clang|extra)_(example|executable|library|loadable_module|target|tool|unittest|utility))\s*\(\s*\w+')
    try:
        while True:
            if len(jobs) < njobs:
                jobs.append(xgrep_gen.next())
                continue
            assert len(jobs) > 0
            for fn_tuple in xgrep_cmake_process(jobs.pop(0)):
                yield fn_tuple
    except StopIteration:
        for grep_job in jobs:
            for fn_tuple in xgrep_cmake_process(grep_job):
                yield fn_tuple

# Pass 0: Collect files and CMakeLists.
cmake_target_names = {}
if opts.verbose:
    sys.stderr.write("====traverse\n")
for filepath,name in xgrep_cmake(walk(users, includes)):
    dir = os.path.dirname(filepath)
    if dir not in cmake_targets:
        continue
    assert os.path.dirname(filepath) not in cmake_target_names
    cmake_target_names[os.path.dirname(filepath)] = name

all_files_set = frozenset(all_files)
xrefs = {}

# Pass 1: Fill includee
if opts.verbose:
    sys.stderr.write("====xrefs\n")
for filepath,includees in xgrep(iter(all_files)):
    if os.path.dirname(filepath) in excluded_dir:
        if opts.verbose:
            sys.stderr.write("\tExcluding <%s>\n" % os.path.dirname(filepath))
        continue

    m = re_header_suffixes.match(filepath)
    isTD = m and (m.group(1) == 'td')

    # update deps
    #print "%s:%s" % (filepath,includees)
    assert filepath not in deps
    deps[filepath] = set()

    for inc in includees:
        inc_filepath = None
        if inc in include_files:
            inc_filepath = include_files[inc]
        else:
            dir = os.path.dirname(filepath)
            while dir:
                if dir in includes:
                    break
                if dir in users:
                    break
                f = os.path.normpath(os.path.join(dir, inc))
                if f in all_files_set:
                    inc_filepath = f
                    break
                dir = os.path.dirname(dir)

        if inc_filepath:
            # 場所を特定できている
            if inc_filepath not in xrefs:
                xrefs[inc_filepath] = set()
            xrefs[inc_filepath].add(filepath)
            if inc_filepath in deps:
                # すでに解析済み
                # 依存をマージする(自分自身は取り除く)
                deps[filepath].update(deps[inc_filepath])
                if isTD:
                    deps[filepath].add(inc_filepath)
                else:
                    deps[filepath].discard(filepath)
                for f in deps[inc_filepath]:
                    xrefs[f].add(filepath)
            else:
                deps[filepath].add(inc_filepath)
        else:
            # 未解決なので表示名だけ足しておく
            if inc.endswith(".h"):
                sys.stderr.write("NOTFOUND %s in %s\n" % (inc, filepath))
            if inc not in xrefs:
                xrefs[inc] = set()
            xrefs[inc].add(filepath)
            deps[filepath].add(inc)

    #assert isTD or filepath not in deps[filepath]
    assert filepath not in deps[filepath]

    # Resolve xref
    if filepath in xrefs:
        for xf in xrefs[filepath]:
            if xf == filepath:
                continue
            assert xf in deps
            deps[xf].update(deps[filepath])
            for f in deps[filepath]:
                xrefs[f].add(xf)
            assert filepath in deps[xf]
            if not isTD:
                deps[xf].remove(filepath)

if opts.verbose:
    sys.stderr.write("====tddeps\n")
out.write("######## Dependencies of *.td\n")
for k,v in sorted(deps.items()):
    m = re.match(r'.+/([^/]+\.td)$', k)
    if not m:
        continue
    out.write("set(TDDEPS_%s\n" % get_name(m.group(1)))
    # Add k for TableGen.cmake to distinguish whether k is recognized
    # as 'known' tdfile.
    v.discard(k)
    for td in [k]+sorted(v):
        out.write("  %s\n" % with_dir_names(td))
    out.write("  %s)\n" % parent_scope)

if opts.verbose:
    sys.stderr.write("====groups\n")
out.write("\n######## Groups\n")
allset = set()
all_tds = set()
for dir,files in sorted(cmake_targets.items()):
    if dir not in cmake_target_names:
        continue
    target_name = cmake_target_names[dir]
    exts = {}
    for filepath in files:
        x = frozenset(deps[filepath])
        if x not in exts:
            exts[x] = set()
        exts[x].add(filepath)

    out.write("# %s(%s) (%d):\n" % (target_name, with_dir_names(dir), len(exts)))
    out.write("set(TDDEPSET_%s\n" % target_name)
    for k in sorted(exts.keys()):
        name_full = get_group_name(k)
        group_name = get_group_name_hashed(k)
        if group_name == name_full:
            out.write("  %s\n" % group_name)
        else:
            out.write("  %s # %s\n" % (group_name, name_full))
    out.write("  %s)\n" % parent_scope)
    assert exts

    for k,v in sorted(exts.items()):
        name_full = get_group_name(k)
        group_name = get_group_name_hashed(k)
        if k in allset:
            if name_full != group_name:
                out.write("# %s\n" % name_full)
        else:
            out.write("set(TDDEPS_%s\n" % group_name)
            for f in sorted(k):
                tdn = "td.%s" % os.path.basename(f)
                out.write("  %s\n" % tdn)
                all_tds.add(tdn)
            out.write("  %s)\n" % parent_scope)
            allset.add(k)

        out.write("set(TDDEPS_%s_%s\n" % (target_name, group_name))
        for i in sorted(v):
            out.write("  %s\n" % os.path.relpath(i, dir))
        out.write("  %s)\n" % parent_scope)

out.write("\nset(TDDEPS_MANAGED_FILES\n")
for k in sorted(all_tds):
    out.write("  %s\n" % k)
out.write("  PARENT_SCOPE)\n")

if opts.verbose:
    sys.stderr.write("====stamps\n")

out.write("\n######## Stamps\n")

srcs_in_dir = {}
all_stamps = {}
root_stamps = [None] * len(users)

# Traverse and construct tree.
# FIXME: It should be "referenced only".
for filepath in all_mtimes.keys():
    assert filepath in all_files_set
    #assert filepath in deps
    dir = os.path.dirname(filepath)
    k = with_dir_names(dir)
    if k not in srcs_in_dir:
        srcs_in_dir[k] = set()
    srcs_in_dir[k].add(filepath)

    # Try to add Parent
    #while dir not in users:
    while True:
        stamp_name = "${SRCS_%s}" % hashlib.sha1(k).hexdigest()
        all_stamps[stamp_name] = k
        if dir in users:
            root_stamps[users.index(dir)] = stamp_name
            break
        dir = os.path.dirname(dir)
        k = with_dir_names(dir)
        if k not in srcs_in_dir:
            srcs_in_dir[k] = set()
        if stamp_name in srcs_in_dir[k]:
            break
        srcs_in_dir[k].add(stamp_name)

if opts.stampdir:
    assert os.path.isdir(opts.stampdir)
    stamp_filepaths = ""
    # Generate stamps order by bottom-up.
    for dir in sorted(sorted(srcs_in_dir.keys()), key=lambda dir: -len(dir)):
        hash = hashlib.sha1(dir).hexdigest()
        srcs_name = "SRCS_%s" % hash
        stamp_filepath = os.path.join(opts.stampdir, "stamp-%s.tmp" % hash)
        if not opts.infile:
            # touch
            open(os.path.join(opts.stampdir, stamp_filepath), "w").close()
        stamp_path = with_dir_names(stamp_filepath)
        stamp_filepaths += "  %s # %s\n" % (stamp_path, dir)

        out.write("# %s\nset(%s\n" % (dir, srcs_name))
        for f in sorted(iter(srcs_in_dir[dir])):
            if f not in all_stamps:
                #assert f in deps
                out.write("    %s\n" % with_dir_names(f))
        for f in sorted(iter(filter(lambda f: f in all_stamps, srcs_in_dir[dir])), key=lambda f: all_stamps[f]):
            # It should not be reference to stamp-*.tmp, but list of children themselves.
            # To parallelize build.
            out.write("    %s # %s\n" % (f, all_stamps[f]))
        out.write("""  )
add_custom_command(OUTPUT %s
  COMMAND touch %s
  DEPENDS ${%s}
  COMMENT "Touched: \\%s")

"""
                  % (stamp_path, stamp_path, srcs_name, dir))

    # Write depends to stamps.
    out.write("set(STAMPS_DEPENDS\n")
    # Depend on all source files.
    # Note: each souce file may be "phony".
    for root_stamp_name in root_stamps:
        if root_stamp_name:
            out.write("  %s # %s\n" % (root_stamp_name, all_stamps[root_stamp_name]))
    # Depend on all stamp file.
    out.write(stamp_filepaths)
    out.write("  )\n")

if outfile:
    #out.write("#XXX\n")
    out.close()

# FIXME: It might be "database file"
if opts.infile and outfile:
    diffopt = "-abq"
    if opts.verbose:
        diffopt = "-abu"
    if subprocess.call(["diff", diffopt, opts.infile, outfile]):
        if action:
            sys.exit(subprocess.call(action))
        else:
            subprocess.call(["cp", "-uv", outfile, opts.infile])
    else:
        if opts.verbose:
            sys.stderr.write("NO UPDATE.\n")
else:
    if opts.verbose:
        sys.stderr.write("====Finished.\n")
