#! /bin/sh

# finish-swig-Python.sh
#
# For the Python script interpreter (external to liblldb) to be able to import
# and use the lldb module, there must be two files, lldb.py and _lldb.so, that
# it can find. lldb.py is generated by SWIG at the same time it generates the
# C++ file.  _lldb.so is actually a symlink file that points to the 
# LLDB shared library/framework.
#
# The Python script interpreter needs to be able to automatically find 
# these two files. On Darwin systems it searches in the LLDB.framework, as
# well as in all the normal Python search paths.  On non-Darwin systems
# these files will need to be put someplace where Python will find them.
#
# This shell script creates the _lldb.so symlink in the appropriate place,
# and copies the lldb.py (and embedded_interpreter.py) file to the correct
# directory.
#

# SRC_ROOT is the root of the lldb source tree.
# TARGET_DIR is where the lldb framework/shared library gets put.
# CONFIG_BUILD_DIR is where the build-swig-Python-LLDB.sh  shell script 
#           put the lldb.py file it was generated from running SWIG.
# PYTHON_INSTALL_DIR is where non-Darwin systems want to put the .py and .so
#           files so that Python can find them automatically.
# debug_flag (optional) determines whether or not this script outputs 
#           additional information when running.

SRC_ROOT=$1
TARGET_DIR=$2
CONFIG_BUILD_DIR=$3
PYTHON_INSTALL_DIR=$4
debug_flag=$5

# Make sure SDKROOT is not set, since if it is this is an iOS build where python
# is disabled
if [ "x$SDKROOT" = "x" ] ; then

if [ -n "$debug_flag" -a "$debug_flag" == "-debug" ]
then
    Debug=1
else
    Debug=0
fi

OS_NAME=`uname -s`
PYTHON_VERSION=`/usr/bin/python --version 2>&1 | sed -e 's,Python ,,' -e 's,[.][0-9],,2' -e 's,[a-z][a-z][0-9],,'`


if [ $Debug == 1 ]
then
    echo "The current OS is $OS_NAME"
    echo "The Python version is $PYTHON_VERSION"
fi

#
#  Determine where to put the files.

if [ ${OS_NAME} == "Darwin" ]
then
    # We are on a Darwin system, so all the lldb Python files can go 
    # into the LLDB.framework/Resources/Python subdirectory.

    if [ ! -d "${TARGET_DIR}/LLDB.framework" ]
    then
        echo "Error:  Unable to find LLDB.framework" >&2
        exit 1
    else
        if [ $Debug == 1 ]
        then
            echo "Found ${TARGET_DIR}/LLDB.framework."
        fi
    fi

    # Make the Python directory in the framework if it doesn't already exist

    framework_python_dir="${TARGET_DIR}/LLDB.framework/Resources/Python"
else
    # We are on a non-Darwin system, so use the PYTHON_INSTALL_DIR argument,
    # and append the python version directory to the end of it.  Depending on
    # the system other stuff may need to be put here as well.

    framework_python_dir="${PYTHON_INSTALL_DIR}/python${PYTHON_VERSION}"
fi

#
# Look for the directory in which to put the Python files;  if it does not
# already exist, attempt to make it.
#

if [ $Debug == 1 ]
then
    echo "Python files will be put in ${framework_python_dir}"
fi

if [ ! -d "${framework_python_dir}" ]
then
    if [ $Debug == 1 ]
    then
        echo "Making directory ${framework_python_dir}"
    fi
    mkdir -p "${framework_python_dir}"
else
    if [ $Debug == 1 ]
    then
        echo "${framework_python_dir} already exists."
    fi
fi

if [ ! -d "${framework_python_dir}" ]
then
    echo "Error: Unable to find or create ${framework_python_dir}" >&2
    exit 1
fi

# Make the symlink that the script bridge for Python will need in the
# Python framework directory

if [ ! -L "${framework_python_dir}/_lldb.so" ]
then
    if [ $Debug == 1 ]
    then
        echo "Creating symlink for _lldb.so"
    fi
    if [ ${OS_NAME} == "Darwin" ]
    then
        cd "${framework_python_dir}"
        ln -s "../../LLDB" _lldb.so
    else
        cd "${TARGET_DIR}"
        ln -s "./LLDB" _lldb.so
    fi
else
    if [ $Debug == 1 ]
    then
        echo "${framework_python_dir}/_lldb.so already exists."
    fi
fi

# Copy the python module (lldb.py) that was generated by SWIG 
# over to the framework Python directory
if [ -f "${CONFIG_BUILD_DIR}/lldb.py" ]
then
    if [ $Debug == 1 ]
    then
        echo "Copying lldb.py to ${framework_python_dir}"
    fi
    cp "${CONFIG_BUILD_DIR}/lldb.py" "${framework_python_dir}"
else
    if [ $Debug == 1 ]
    then
        echo "Unable to find ${CONFIG_BUILD_DIR}/lldb.py"
    fi
fi

# Copy the embedded interpreter script over to the framework Python directory
if [ -f "${SRC_ROOT}/source/Interpreter/embedded_interpreter.py" ]
then
    if [ $Debug == 1 ]
    then
        echo "Copying embedded_interpreter.py to ${framework_python_dir}"
    fi
    cp "${SRC_ROOT}/source/Interpreter/embedded_interpreter.py" "${framework_python_dir}"
else
    if [ $Debug == 1 ]
    then
        echo "Unable to find ${SRC_ROOT}/source/Interpreter/embedded_interpreter.py"
    fi
fi

# Copy the C++ STL formatters over to the framework Python directory
if [ -f "${SRC_ROOT}/examples/synthetic/gnu_libstdcpp.py" ]
then
    if [ $Debug == 1 ]
    then
        echo "Copying gnu_libstdcpp.py to ${framework_python_dir}"
    fi
    cp "${SRC_ROOT}/examples/synthetic/gnu_libstdcpp.py" "${framework_python_dir}"
else
    if [ $Debug == 1 ]
    then
        echo "Unable to find ${SRC_ROOT}/examples/synthetic/gnu_libstdcpp.py"
    fi
fi

fi

exit 0

