call bitsum-sign x64\release\groupextend.exe
call bitsum-sign release\groupextend.exe
pkzipc -add dist\groupextend.zip x64\release\groupextend.exe
pkzipc -add dist\groupextend32.zip release\groupextend.exe
scp dist\groupextend.zip jeremy@az.bitsum.com:/var/www/vhosts/bitsum.com/files/groupextend.zip
scp dist\groupextend32.zip jeremy@az.bitsum.com:/var/www/vhosts/bitsum.com/files/groupextend32.zip