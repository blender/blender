Elman neuro node layer 1
========================

 Neuro network node
  This node teachable. You may teach him rules, that he understand himself. Just put data and correct answer. When displace answer, he will find right answer himself.
  Input data. Inserting many objects - output many objects. Inserting one object with many parameters - output one object.
  Always insert constant numbers count of parameters, otherwise it will reset neuro data and start every time from beginning. Keep constant numbers count.

- coef_learning - learning speed coeffitient, accuracy influence (less - more accuracy);
- gisterezis - spread of input and etalon data;
- maximum - maximum number input (better define little overhang number);
- cycles - passes on one object;
- A layer - input layer cores (and it is number of objects);
- B layer - inner layer cores - more - smarter (overlearning is bad too);
- C layer - output layer cores - numbers quantity in output;
- epsilon - inner variable - argument offset in passes 'cycles' (not much influence totally);
- lambda - holding coefficient, to preserve data flooding;
- threshold - inner variable - defines reasonability limit in passes 'cycles' (not much influence totally).
