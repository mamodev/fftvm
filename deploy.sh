set -euo pipefail

pip install build twine
python -m build

pip install ./dist/fftvm-0.1.0-cp310-cp310-linux_x86_64.whl
(cd / && python3 -c "import fftvm; print('Success!')")
pip uninstall -y fftvm

twine upload dist/fftvm-0.1.0.tar.gz