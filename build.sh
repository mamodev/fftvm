
if [[ -d "venv_test/bin/activate" ]]; then 
    source venv_test/bin/activate   
else
    python3 -m venv venv_test
    source venv_test/bin/activate   
    pip install --upgrade pip setuptools wheel
fi

pip install . -v