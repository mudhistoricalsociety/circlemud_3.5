#27600
Dump - 27608~
2 h 100
~
%echo% %object.shortdesc% vanishes in a puff of smoke!
%send% %actor% You are awarded for outstanding performance.
%echoaround% %actor% %actor.name% has been awarded for being a good citizen.
eval value %object.cost% / 10
%purge% %object%
if %value% > 50
  eval value 50
elseif %value% < 1
  eval value 1
end
if %actor.level% < 3
  nop %actor.exp(%value%)%
else
  nop %actor.gold(%value%)%
end
~
$~
