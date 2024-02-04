CS Project 4 - Process Scheduling

INFO:
        - The simulated os generates new processes for 3 real life seconds and uses a scheduler to check if processes are blocked or terminating.


TO COMPILE:     make

TO RUN:         ./oss -h -n [# of workers] -s [# of simultaneous workers] -t [timeToLauchNewChild] -f [logFile name]

                example1:       ./oss -n 5 -s 3 -t 500000000 -f fileName.txt
                example2:       ./oss -n 10 -s 5 -t 500000000 -f fun.txt

FOR HELP: ./oss -h
