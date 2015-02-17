Funkylady
==============

Purpose
--------------
Funkylady is an implementation of the board game known as Othello.

Usage
--------------
$ ./funkylady [-ccolor black/white] [-ttime <minutes>] [-load <filename>] [-from <movenr>] [-cachesize <numelems>] [-deterministic true/false] [-opponent <name>] [-nodump] [-pipes <pipein> <pipeout>]

In-game commands:
- <enter>                         List possible moves
- ?                               List available commands
- quit                            Quit the game
- undo [<nummoves>]               Undo move(s)
- save [<filename>]               Save current board position and config
- load [<filename> [<frommove>]]  Load board position and config, replay moves
- help [<targettime>]             Let the computer suggest a good move

Run a performance benchmark with a predefined position. Below is from my Core2 Duo @ 2GHz:
$ ./funkylady -load pos
Welcome to Funkylady, the Othello contestor
Copyright 1993 Ola Liljedahl, all rights reserved
    a b c d e f g h
   -----------------
1 | . . . . . . . . | 1
2 | . . . . . . . . | 2
3 | . . . O O X . . | 3
4 | . . X X X X . . | 4
5 | . . O X X O . . | 5
6 | . . . . X X O . | 6
7 | . . . . . X . . | 7
8 | . . . . . . . . | 8
   -----------------
    a b c d e f g h
             discs   time
   human (X):  10    0:00
computer (O):   5    0:00
Search time target 20.00 secs
ply ss  bm  value nodes hits  time  nodes/s
 3   2  g5    -4    901   0%  ~0
 4   2  g5     2   1962   0%  ~0
 5   2  g5    -6   6653   0%  ~0
 6   2  g5     2  21955   0%  ~0
 7   2  g5   -10  87166   0%   0.01   8716600
 8   2  e7     0 871738   0%   0.20   4358690
 9   2  g5    -9 2465306   0%   0.57   4325098
10   2  c3    -1 16465179   0%   3.79   4344374
11   2  c3   -12 40039805   0%   9.33   4291511

Design
--------------
TODO

Author
--------------
Ola Liljedahl ola.liljedahl@gmail.com

Background
--------------
I wrote Funkylady back in 1993-1994 as a spin-off from the functional model (thus the name) of "Desdemona". Desdemona was our HW+SW (386 PC + ISA board with FPGA's) implementation of the Othello board game that was developed in the 1993 Computer Design Project (DSK - Datorsystemkonstruktion) at Lunds Tekniska Högskola.

I also contributed to the original HW rough architecture proposal for Desdemona which was used for planning the actual development project. Desdemona participated in the 1st International Computer Othello Tournament held in Paderborn 1993 where it shared 6th place (of 12 participants).

The DSK class was lead by prof. Lars Philipson and used the slanted wave principle ("sneda vågens princip"). This method breaks down the development into a number of (dependent) subprojects that can be developed in parallel, thus speeding up development. Unlike modern agile projects, the slanted wave principle requires detailed planning but this only concerns *what* functionality shall be completed at well-defined milestones, not how to implement the functionality. The advantage of the slanted wave principle is that very complex designs can be developed in a bounded time (you still have to work very hard).

Those were the days...
