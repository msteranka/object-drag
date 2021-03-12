import json, argparse

def is_valid_path(path):
    return path != ''

def print_backtrace(trace):
    for x in trace:
        if is_valid_path(x['path']):
            print('\t\t' + str(x['path']) + ':' + str(x['line']))
        else:
            print('\t\t(NIL)')

def print_fragment(frag, ftime, start, end):
    print('\t\t' + str(start) + '-' + str(end) + ' -- Drag: ' + str(ftime - f['atime']) + ', Last accessed: ' + str(f['atime']) + ', ', end = '')
    # print('\t\t' + str(start) + '-' + str(end) + ' -- Last accessed: ' + str(f['atime']) + ', Drag: ' + str(ftime - f['atime']) + ', ', end = '')
    if is_valid_path(f['apath']):
        print('Accessed @ ' + str(f['apath']) + ':' + str(f['aline']))
    else:
        print('Accessed @ (NIL)')

parser = argparse.ArgumentParser()
parser.add_argument('--input', type = str, help = 'The path to the output file')
args = parser.parse_args()
json_file = '../src/drag.json'
if(args.input != None):
    json_file = args.input

with open(json_file, 'r') as f:
    data = json.load(f)

metadata = data['metadata']
print('Maximum backtrace depth: ' + str(metadata['depth']))
print('Fragment size: ' + str(metadata['fragsize']) + '\n')

for x in data['objs']:
    print('Object ' + hex(x['addr']) + ':')
    print('\tSize: ' + str(x['size']) + ' bytes')
    print('\tmalloc thread ID: ' + str(x['mtid']))
    print('\tfree thread ID: ' + str(x['ftid']))
    print('\tmalloc Backtrace:')
    print_backtrace(x['mtrace'])
    print('\tfree Backtrace:')
    print_backtrace(x['ftrace'])
    print('\tFree time: ' + str(x['ftime']) + ' bytes')
    print('\tFragments:')
    offset = 0
    for f in x['frags']:
        start = offset
        if offset + 8 > x['size']:
            end = x['size']
        else:
            end = offset + 8
        print_fragment(f, x['ftime'], start, end)
        offset += metadata['fragsize']
