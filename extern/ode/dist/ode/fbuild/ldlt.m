function [L,d] = ldlt(A)

n=length(A);
d=zeros(n,1);

d(1) = 1/A(1,1);
for i=2:n
  for j=2:i-1
    A(i,j) = A(i,j) - A(j,1:j-1) * A(i,1:j-1)';
  end
  sum = 0;
  for j=1:i-1
    q1 = A(i,j);
    q2 = q1 * d(j);
    A(i,j) = q2;
    sum = sum + q1*q2;
  end
  d(i) = 1/(A(i,i) - sum);
end

L=A;
for i=1:n
  L(i,i:n)=zeros(1,n+1-i);
  L(i,i)=1;
end
d = d .\ 1;
