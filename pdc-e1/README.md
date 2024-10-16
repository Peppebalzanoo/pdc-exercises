# Parallel and Distributed Computing
Exercise 1
### The following exercise aims to solve the "Sum of N Numbers" problem in a parallel computing environment with MIMD-DM architecture using MPI
The parallel computing environment used for solving the problem employs MIMD-DM architecture (Multiple Instruction Multiple Data - Distributed Memory). Specifically, this type of architecture includes multiple distinct processing units (processors) that simultaneously execute separate computations on different data streams. Each processing unit has its own local memory and can execute its instructions independently of the other units. If a unit needs to access data stored in another unit, it must request access through techniques such as Message Passing Interface.

Specifically, given $a_0, ..., a_{n−1}$ ∈ R, the sum S of the $a_i$ is defined as: <br>
<p align="center"> $S = a_0+,...,+a_{n-1}\ =\ \sum\limits_{i=0}^{n-1}a_i$</p>

Solving the problem in a parallel computing environment is based on the idea of decomposing the problem into smaller-sized sub-problems and solving them simultaneously on multiple computing units/processors. 

Let $X$ be our problem with a size of $n$. The concept involves dividing problem $X$ into $p$ sub-problems (given that we have $p$ processors) of size $n/p$ and solving them concurrently on the $p$ processors. Once the results of the $p$ sub-problems are obtained, they need to be combined in a suitable manner to obtain the total sum.

<br>

## Data Reading and Generation
In the implementation, data is expected to be either read from input sources (if $n <= 20$) or generated (if $n > 20$) using a pseudo-random number generator. It's important to note that data generation does not reflect reality because, in real parallel computing environments, data is not generated but is already available for reading and utilization.

## Data Distribution Among Processors
This phase involves the fair and balanced distribution of data across various processors, labeled as $P_1, P_2, ..., P_n$. There are different techniques for achieving load balance, and the one used in this work is data partitioning, which allows for dividing the dataset in a way that each processor receives a similar amount of data for processing. This is particularly useful when the workload is known in advance, and the data can be divided fairly. An equitable and balanced load distribution will ensure:
  1. Optimal performance
  2. Increased efficiency in resource utilization
  3. Reduction in waiting times
  4. Scalability
  5. Enhanced reliability

## Strategy 1
Once each processor $P_i$ has computed the partial sums $S_i$, each partial sum will be sent to a specific process (in our case, $P_0$) that will sequentially sum these partial sums to obtain the total sum. In this case, the total sum is stored on a single processor, $P_0$.
## Strategy 2
The second strategy employs a hierarchical approach based on a tree structure to perform the cumulative sum of partial values. This tree is constructed using a "bottom-up" approach, starting from the nodes with partial sums and moving up to the root with the total sum.

1. In the first time step, each processor calculates its partial sum
2. Subsequently, the set of processors, each with its own partial sum, is virtually divided into pairs <br>

Each pair consists of two processors:
  - The processor that transmits its partial sum, which is the processor with the larger ID
  - The processor that receives the transmitted partial sum and adds it to its own partial sum, which is the processor with the smaller ID

This process is iteratively repeated until the total sum is contained in a single processor.

## Strategy 3
The third strategy is very similar to the second one, but at each communication step, all processors are involved. This means that every processor in each pair both sends and receives partial sums. The purpose of this approach is to ensure that, at the end of the algorithm, the total sum is distributed across all processors. However, this strategy can be computationally expensive due to a high number of communications.

MPI: https://en.wikipedia.org/wiki/Message_Passing_Interface

