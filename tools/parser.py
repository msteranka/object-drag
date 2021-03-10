import json, argparse

def is_valid_path(path):
    return path != ''

def print_backtrace(trace):
    for x in trace:
        if is_valid_path(x['path']):
            print('\t\t' + str(x['path']) + ':' + str(x['line']))
        else:
            print('\t\t(NIL)')

parser = argparse.ArgumentParser()
parser.add_argument('--input', type = str, help = 'The path to the output file')
args = parser.parse_args()
json_file = '../src/drag.json'
if(args.input != None):
    json_file = args.input

with open(json_file, 'r') as f:
    data = json.load(f)

metadata = data['metadata']
print('Maximum backtrace depth: ' + str(metadata['depth']) + '\n')

for x in data['objs']:
    print('Object ' + hex(x['addr']) + ':')
    print('\tDrag: ' + str(x['ftime'] - x['atime']) + ' bytes')
    print('\tFree time: ' + str(x['ftime']) + ' bytes')
    print('\tLast access time: ' + str(x['atime']) + ' bytes')
    if is_valid_path(x['apath']):
        print('\tLast accessed @ ' + str(x['apath']) + ':' + str(x['aline']))
    else:
        print('\tLast accessed @ (NIL)')
    print('\tSize: ' + str(x['size']) + ' bytes')
    print('\tmalloc thread ID: ' + str(x['mtid']))
    print('\tfree thread ID: ' + str(x['ftid']))
    print('\tmalloc Backtrace:')
    print_backtrace(x['mtrace'])
    print('\tfree Backtrace:')
    print_backtrace(x['ftrace'])
