echo Mapping ivi addresses ...
$IVIPATH/utils/ivimap -4 -p 2001:da8:ff00:: -M -R 4 -l 40 -P 2001:da8:ff00:: -b -L 40 10.0.0.2
$IVIPATH/utils/ivimap -6 -l 40 -L 40 -b fec0::2
echo Done.


