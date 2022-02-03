#!/bin/bash

if [[ ${OS_TYPE} == "doxygen" ]]; then make && sudo make install ; fi
