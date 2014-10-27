#!/usr/bin/env python

import os
import re
import subprocess
import sys

subs = {
    'clang': 'cfe',
    }

re_hash = re.compile(r'^([0-9A-Fa-f]{40,})')

p_mktree = subprocess.Popen(
    ['git', 'mktree', '--batch'],
    stdin=subprocess.PIPE,
    stdout=subprocess.PIPE,
    )

def mktree(commit, parent=None, msg=''):
    re_lstree = re.compile(r'^040000 tree (?P<hash>[0-9A-Fa-f]{40,})\t(?P<tree>\S+)')

    p_lstree = subprocess.Popen(
        ['git', 'ls-tree', commit],
        stdout=subprocess.PIPE,
        )

    repos = {}
    for line in p_lstree.stdout:
        m = re_lstree.search(line.decode())
        assert m
        p_mktree.stdin.write(("040000 tree %s\t%s\n\n" % (m.group('hash'), 'trunk')).encode())
        p_mktree.stdin.flush()
        mm = re_hash.search(p_mktree.stdout.readline().decode())
        assert mm
        repos[m.group('tree')] = mm.group(1)

    p_lstree.wait()

    for (tree,hash) in repos.items():
        if tree in subs:
            tree = subs[tree]
        p_mktree.stdin.write(("040000 tree %s\t%s\n" % (hash,tree)).encode())
    p_mktree.stdin.write("\n".encode())
    p_mktree.stdin.flush()
    tree_root = re_hash.search(p_mktree.stdout.readline().decode())
    assert tree_root
    tree_root = tree_root.group(1)

    commit_env = dict(os.environ)
    p_cat_commit = subprocess.Popen(
        ['git', 'cat-file', 'commit', commit],
        stdout=subprocess.PIPE,
        )
    assert p_cat_commit
    msg_body = p_cat_commit.stdout.readlines()

    p_cat_commit.wait()
    #author NAKAMURA Takumi <geek4civic@gmail.com> 1414065439 +0900
    re_commit_raw = re.compile(r'^(?P<k>author|committer)\s+(?P<name>.+)\s+<(?P<email>.+)>\s+(?P<date>\d+)')

    while msg_body:
        line = msg_body.pop(0).decode();
        if line == '\n':
            break
        kv = re_commit_raw.search(line)
        if not kv:
            continue
        k = kv.group('k').upper()
        commit_env['GIT_%s_NAME' % k] = kv.group('name')
        commit_env['GIT_%s_EMAIL' % k] = kv.group('email')
        commit_env['GIT_%s_DATE' % k] = kv.group('date') # It drops tz.

    commit_tree_arg = ['git', 'commit-tree', tree_root]
    if parent:
        commit_tree_arg.extend(['-p', parent])
    p_commit_tree = subprocess.Popen(
        commit_tree_arg,
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        env=commit_env
        )
    assert p_commit_tree
    commit_result = p_commit_tree.communicate((msg.encode() + b''.join(msg_body)))
    p_commit_tree.wait()
    commit_hash = re_hash.search(commit_result[0].decode())
    return commit_hash.group(1)

root = None
for rev in sys.argv[1:]:
    p_revlist = subprocess.Popen(
        ['git', 'rev-list', '--no-walk', '--reverse', rev],
        stdout=subprocess.PIPE,
        )
    assert p_revlist
    parent = None
    for line in p_revlist.stdout:
        commit = re_hash.search(line.decode())
        assert commit
        commit = commit.group(1)
        if not root:
            parent = mktree(commit='%s^' % commit, msg='[DO NOT COMMIT] ')
        elif not parent:
            parent = mktree(commit='%s^' % commit, msg='[DO NOT COMMIT] ', parent=root)
        commit = mktree(commit=commit, parent=parent)
        subprocess.call(["git", "log", "--no-walk", "--stat", commit])
        sys.stdout.write("git svn commit-diff -rHEAD %s %s https://llvm.org/svn/llvm-project\n" % (parent,commit))
        root = parent = commit

    p_revlist.wait()

p_mktree.stdin.close()
p_mktree.wait()
