#!/bin/bash

cd mountDir  

my_array=""
for ((i=1; i<=4000; i++)); do
    my_array+="a"
    if ((i % 100 == 0)); then
        my_array+=$'\n'
    fi
done

echo -ne "$my_array" > output.txt