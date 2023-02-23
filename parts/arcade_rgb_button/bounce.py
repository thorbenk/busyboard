from matplotlib import pyplot as plot
import numpy as np

with open("bounce.csv") as f:
    x = [0]
    y = [1]

    for line in f.readlines():
        line = line.strip()
        if line == "":
            continue
        usec, edge = line.split(" ")
        usec = int(usec)
        edge = int(edge)
        
        print(line)

        if edge == 1:
            x.append(usec-1)
            y.append(0)
            x.append(usec)
            y.append(1)
        else:
            x.append(usec-1)
            y.append(1)
            x.append(usec)
            y.append(0)
    
    print(x[0:10])
    print(y[0:10])
    
    x = np.asarray(x)
    x = x / 1000.

    plot.plot(x, y)
    plot.xlabel("msec")
    plot.show()

    

