#!/bin/bash

cd mountDir  

count=1000
for ((i = 1; i <= count; i++)); do
    mkdir dir_$i
done