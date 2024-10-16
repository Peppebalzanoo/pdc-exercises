# Parallel and Distributed Computing
Exercise 3
### The following exercise aims to solve the "Matrix by Matrix Product" problem in a parallel computing environment with MIMD-DM architecture using MPI
The parallel computing environment used for solving the problem employs MIMD-DM architecture (Multiple Instruction Multiple Data - Distributed Memory). Specifically, this type of architecture includes multiple distinct processing units (processors) that simultaneously execute separate computations on different data streams. Each processing unit has its own local memory and can execute its instructions independently of the other units. If a unit needs to access data stored in another unit, it must request access through techniques such as Message Passing Interface.

## Strategy
Broadcast Multiply Rolling (BMR)

MPI: https://it.wikipedia.org/wiki/Message_Passing_Interface
