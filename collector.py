import time
import pickle

events          = [] 
event_cap       = 1000
buffer_size     = 2048
python_log_path = 'C:\\python_log\\python_log.dcart'
driver_log_path = 'C:\\driver_log\\driver_log.dcart'

try:
    # attempt to create a log file for this script
    python_log = open(python_log_path, "x")
    python_log.close()
except:
    # file could not be created
    pass

# opens a file with write permissions for updating in binary mode
python_log = open(python_log_path, "wb+")

try:
    # attempt to create a driver log file (this should fail)
    driver_log = open(driver_log_path, "x")
    driver_log.close()
except:
    # file could not be created
    pass

# opens a file with read permissions in binary mode
driver_log = open(driver_log_path, "rb")

def read_contents(path):
    contents = ''
    result = True
    
    try:
        f        = open(path, "rb") 
        contents = f.read(buffer_size)
    except Exception as e:
        result = False

    return result, contents

def tail_log():
    prev_d   = None
    contents = ''
    result   = True

    while True:
        index = driver_log.tell()
        entry = driver_log.readline()
        
        if entry != '':
            entry        = entry.replace(b'\n',b'')
            entry        = entry.split(b'|')
            entry_length = len(entry)
            
            # drop the event if a duplicate entry exists
            if entry in events:
                continue
            
            # rename events will have four fields: operation, pid, previous path, new path
            # all other events will have three fields: operation, pid, path
            if (3 == entry_length) or (4 == entry_length):
                operation        = entry[0]
                pid              = entry[1]
                path             = entry[2]
                # clean up string and attempt to translate to standard file path
                path             = path.replace(b'\\Device\\HarddiskVolume2\\', b'C:\\')
                path             = path.replace(b'\\??\\', b'')
                
                if 4 == entry_length:
                    new_path  = entry[3]
                    # clean up string and attempt to translate to standard file path
                    new_path  = path.replace(b'\\Device\\HarddiskVolume2\\', b'C:\\')
                    new_path  = path.replace(b'\\??\\', b'')
                
                result, contents = read_contents(path)
                
                # drop the event if the file is empty since we won't be able to derive any meaningful metrics
                if len(contents) == 0:
                    continue
                
                if result:
                    d              = {}
                    d['operation'] = operation
                    d['pid']       = pid
                    d['contents']  = contents
                    
                    if 3 == entry_length:
                        d['path'] = path
                    elif 4 == entry_length:
                        d['prev_path'] = path
                        d['path']      = new_path

                    # drop the event if it is a duplicate of the last seen
                    if d == prev_d:
                        continue
                    
                    # print outs for simple debugging and validation
                    if 3 == entry_length:
                        try:
                            print(d['pid'].decode('utf-8') + ' | ' + d['operation'].decode('utf-8') + ' | ' + d['path'].decode('utf-8'))
                        except:
                            print(d['pid'] + b' | ' + d['operation'] + b' | ' + d['path'])
                    elif 4 == entry_length:
                        try:
                            print(d['pid'].decode('utf-8') + ' | ' + d['operation'].decode('utf-8') + ' | ' + d['prev_path'].decode('utf-8') + ' => ' + d['path'].decode('utf-8'))
                        except:
                            print(d['pid'] + b' | ' + d['operation'] + b' | ' + d['prev_path'] + b' => ' + d['path'])

                    # write pickled dictionary (d) to the python log file
                    pickle.dump(d, python_log)
                    prev_d = d

                    events.append(entry)

            # once event cap is reached, break out of the while loop
            if event_cap == len(events):
                print('Event cap reached!')
                break

        else:
            time.sleep(0.5)
            driver_log.seek(index)

if __name__ == "__main__":
    tail_log()