./create_sym_links.sh
cd s3fnet-lxc/base
bash CLEAN_EXPERIMENT
cd ../../awlsim
python generator.py $1
python config_parser.py
cd ../Projects/$1
cd conf/PLC_Config/; make clean; make; sudo make test
cd ../../../../
chmod -R 777 s3fnet-lxc/experiment-data