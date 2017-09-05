#!/usr/bin/env python2.7

# This program finds all APrinter header files needed by AIpStack and
# copies them into the AIpStack source tree. It must be given the path
# to the APrinter source tree where it looks for included APrinter
# headers.
# 
# Each AIpStack source file is searched for APrinter includes and any
# such included file will itself be examined for APrinter includes
# recursively. This is implemented using breadth-first search.

from __future__ import print_function
import sys
sys.dont_write_bytecode = True
import os
import argparse
import stat
import re
import shutil
import errno

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('-a', '--aprinter-dir', required=True,
                        help='APrinter source code directory')
    args = parser.parse_args()
    
    aipstack_dir = os.path.dirname(os.path.dirname(os.path.realpath(__file__)))
    
    state = {
        'src_dir': os.path.join(aipstack_dir, 'src'),
        'aprinter_dir': args.aprinter_dir,
        'pending_files': set(),
        'processed_files': set(),
        'include_regex': re.compile(r'^ *# *include *<(aprinter/.+)> *$', re.MULTILINE),
    }
    
    collect_files(state, '')
    process_pending_files(state)
    copy_aprinter_files(state)

def collect_files(state, dir_path):
    if dir_path == 'aprinter':
        return
    
    src_dir = state['src_dir']
    full_dir_path = os.path.join(src_dir, dir_path)
    
    for name in os.listdir(full_dir_path):
        path = os.path.join(dir_path, name)
        full_path = os.path.join(src_dir, path)
        mode = os.stat(full_path)[stat.ST_MODE]
        
        if stat.S_ISDIR(mode):
            collect_files(state, path)
        elif stat.S_ISREG(mode):
            if path.endswith('.h'):
                state['pending_files'].add(path)

def process_pending_files(state):
    pending_files = state['pending_files']
    processed_files = state['processed_files']
    
    while len(pending_files) > 0:
        file_path = pending_files.pop()
        processed_files.add(file_path)
        process_file(state, file_path)

def process_file(state, file_path):
    #print('processing {}'.format(file_path))
    
    if file_is_from_aprinter(file_path):
        base_dir = state['aprinter_dir']
    else:
        base_dir = state['src_dir']
    
    full_path = os.path.join(base_dir, file_path)
    
    with open(full_path, 'rb') as f:
        file_content = f.read()
    
    pending_files = state['pending_files']
    processed_files = state['processed_files']
    
    for match in state['include_regex'].finditer(file_content):
        inc_path = match.group(1)
        if inc_path not in pending_files and inc_path not in processed_files:
            pending_files.add(inc_path)

def copy_aprinter_files(state):    
    aprinter_files = filter(file_is_from_aprinter, state['processed_files'])
    
    for file_path in aprinter_files:
        src = os.path.join(state['aprinter_dir'], file_path)
        dst = os.path.join(state['src_dir'], file_path)
        
        print('copying {}'.format(file_path))
        #print('copying {} to {}'.format(src, dst))
        
        mkdir_p(os.path.dirname(dst))
        shutil.copy(src, dst)

def file_is_from_aprinter(file_path):
    return file_path.startswith('aprinter'+os.sep)

def mkdir_p(path):
    try:
        os.makedirs(path)
    except OSError as ex:
        if ex.errno != errno.EEXIST or not os.path.isdir(path):
            raise

if __name__ == '__main__':
    main()
