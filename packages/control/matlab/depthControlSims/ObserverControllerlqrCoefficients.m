% This file creates K and L for the observer controller with LQR
m=20;
c=11.5;
A = [0 1;0 -c/m];
B = [0; 1/m];
C = [1 0];
%K = place(A,B,[-4 -4.1])
K = lqr(A,B,Q,R)
L = (place(A',C',[-22 -22.1]))'