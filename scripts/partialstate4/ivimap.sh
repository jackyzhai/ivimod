echo Mapping ivi addresses ...
$IVIPATH/utils/ivimap -4 -z -n 1.1.1.0 -Z -l 40 -L 40 -M -R 4 -c 10.0.0.2
$IVIPATH/utils/ivimap -6 -z -l 40 -L 40 -Z fec0::2
echo Done.


