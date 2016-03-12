S3FNET_DIR = ../../s3fnet-lxc/base/s3fnet
MODEL_NAME = test
VIRT_SPACE = 10.10.0.0
#DICTIONARY = ../../s3fnet-lxc/base/s3fnet/test/lxc_tests/aux/mydictionary.dml
#NETWORKS   = ../../s3fnet-lxc/base/s3fnet/test/lxc_tests/aux/networks.dml
DICTIONARY = ../temp/mydictionary.dml
NETWORKS   = ../temp/networks.dml
PARAMS     = params.dml

all:	$(MODEL_NAME)-env.dml $(MODEL_NAME)-rt.dml

$(MODEL_NAME)-env.dml:	$(MODEL_NAME).dml
	$(S3FNET_DIR)/dmlenv -b $(VIRT_SPACE) $(MODEL_NAME).dml $(DICTIONARY) $(NETWORKS) $(PARAMS) > $@

$(MODEL_NAME)-rt.dml:   $(MODEL_NAME).dml
	$(S3FNET_DIR)/dmlenv -r all $(DICTIONARY) $(NETWORKS) $(PARAMS) $(MODEL_NAME).dml > $@

test:   $(MODEL_NAME)-env.dml $(MODEL_NAME)-rt.dml
	$(S3FNET_DIR)/s3fnet $(DICTIONARY) $(NETWORKS) params.dml test-env.dml test-rt.dml test.dml 

clean:
	rm -f $(MODEL_NAME)-env.dml $(MODEL_NAME)-rt.dml *~ .tmpdat*
