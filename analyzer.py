import time
import pickle
import os
import sys
import math

score_threshold = 5.0

bmp = [0x42, 0x4D]
doc = [0xD0, 0xCF, 0x11, 0xE0, 0xA1, 0xB1, 0x1A, 0xE1]
gif = [0x47, 0x49, 0x46, 0x38]
jpg = [0xFF, 0xD8, 0xFF]
mz  = [0x4D, 0x5A]
pdf = [0x25, 0x50, 0x44, 0x46]
pk  = [0x50, 0x4B]
png = [0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A]
zip = [0x1F, 0x8B]

known_headers = {}
known_headers['bmp']  = ''.join(map(chr,bmp)).encode()
known_headers['dll']  = ''.join(map(chr,mz)).encode()
known_headers['doc']  = ''.join(map(chr,doc)).encode()
known_headers['docx'] = ''.join(map(chr,pk)).encode()
known_headers['exe']  = ''.join(map(chr,mz)).encode()
known_headers['gif']  = ''.join(map(chr,gif)).encode()
known_headers['jpg']  = ''.join(map(chr,jpg)).encode()
known_headers['pdf']  = ''.join(map(chr,pdf)).encode()
known_headers['png']  = ''.join(map(chr,png)).encode()
known_headers['pptx'] = ''.join(map(chr,pk)).encode()
known_headers['xlsx'] = ''.join(map(chr,pk)).encode()
known_headers['zip']  = ''.join(map(chr,zip)).encode()

entropy_max = {}
entropy_max['bmp']  = 7.5
entropy_max['c']    = 7.0
entropy_max['cpp']  = 7.0
entropy_max['dll']  = 7.5
entropy_max['doc']  = 7.5
entropy_max['docx'] = 7.5
entropy_max['exe']  = 7.5
entropy_max['gif']  = 7.5
entropy_max['h']    = 7.0
entropy_max['jpg']  = 7.5
entropy_max['pdf']  = 7.5
entropy_max['png']  = 7.5
entropy_max['pptx'] = 7.5
entropy_max['rtf']  = 7.0
entropy_max['txt']  = 7.0
entropy_max['xlsx'] = 7.5
entropy_max['zip']  = 7.5

def analyze(log_path):
    f                  = open(log_path, "rb")
    total_files        = 0
    system_alert_score = 0.0
    original_data      = None
    
    while True:
        try:
            original_data = pickle.load(f)
        except:
            break
        
        total_files += 1
        
        # individual event analysis
        
        try:
            original_data['path']      = original_data['path'].decode('utf-8')
            original_data['operation'] = original_data['operation'].decode('utf-8')
            original_data['pid']       = original_data['pid'].decode('utf-8')
        except:
            break

        pid            = original_data['pid']
        file_name      = os.path.basename(original_data['path'])
        file_extension = os.path.splitext(original_data['path'])[1][1:]

        print('=' * 20)
        print('pid: ', pid)
        print('file_name: ', file_name)
        print('operation: ', original_data['operation'])
        print('original_data contents length: ', len(original_data['contents']))

        if original_data['operation'] == 'RENAME':
            prev_path           = original_data['prev_path'].decode('utf-8')
            prev_file_extension = os.path.splitext(prev_path)[1][1:]
            print('previous extension: ', prev_file_extension)

        # 1) header mismatch

        if original_data['operation'] == 'RENAME':
            if prev_file_extension in known_headers:
                if len(original_data['contents']) >= len(known_headers[prev_file_extension][1:]):
                    if not original_data['contents'].startswith(known_headers[prev_file_extension][1:]):
                        print('*** renamed file header mismatch ***')
                        system_alert_score += 4.0
        elif file_extension in known_headers:
            if len(original_data['contents']) >= len(known_headers[file_extension][1:]):
                if not original_data['contents'].startswith(known_headers[file_extension][1:]):
                    print('*** header mismatch ***')
                    system_alert_score += 2.0
        
        # 2) entropy analysis
        entropy = calculate_entropy(original_data['contents'])
        print('entropy: ', entropy)

        if original_data['operation'] == 'RENAME':
            if prev_file_extension in entropy_max:
                print('*** renamed file exceeds expected entropy max ***')
                system_alert_score += 4.0
        elif file_extension in entropy_max:
            if entropy > entropy_max[file_extension]:
                print('*** file exceeds expected entropy max ***')
                system_alert_score += 2.0
        
    print('')
    print('-' * 20)
    print('Total Files Analyzed: ', total_files)
    print('Total Alert Score: ', system_alert_score)
    if score_threshold < system_alert_score:
        print('***** Alert Score Exceeded Threshold *****')

def calculate_entropy(data):
    if len(data) == 0:
        return 0.0
    
    entropy = 0.0
    
    for x in range(256):
        p_x = float(data.count(x))/len(data)
        if p_x > 0:
            entropy += - p_x*math.log(p_x, 2)

    return entropy

if __name__ == "__main__":
    if len(sys.argv) == 1:
        log_path = "C:\\python_log\\python_log.dcart"
    else:
        log_path = sys.argv[1]
    
    if os.path.exists(log_path):
        analyze(log_path)
