Ten-Young Guh
tguh3@binghamton.edu
B00397548

Extra credit
============
Copy construction and assignment run in O(n).
I did not write extra test code to demonstrate the time complexity,
but test-scaling already demonstrates it.

Notes
=====
I also implement move construction and assignment.

I allocate the links directly inside the node,
so each new node needs at least just one heap allocation.
Were I to declare a vector in the node,
each new node would need at least two heap allocations.

In an attempt to ease development of a deterministic skip list,
for which I lacked the time, I tried using vectors,
and performance worsened by a factor of 2 or more.
