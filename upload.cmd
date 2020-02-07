bitsum-sign x64\release\groupextend.exe
bitsum-sign release\groupextend.exe
rar a -afzip dist\groupextend.zip x64\release\groupextend.exe
rar a -afzip dist\groupextend32.zip release\groupextend.exe
scp dist\groupextend.zip jeremy@az.bitsum.com:/var/www/vhosts/bitsum.com/files/groupextend.zip
scp dist\groupextend32.zip jeremy@az.bitsum.com:/var/www/vhosts/bitsum.com/files/groupextend32.zip