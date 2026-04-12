#!/bin/bash
# install_to_env.sh
if [[ ! -f ".env" ]]; then
    echo "Error: .env file not found."
    exit 1
fi
source .env
pip install .
