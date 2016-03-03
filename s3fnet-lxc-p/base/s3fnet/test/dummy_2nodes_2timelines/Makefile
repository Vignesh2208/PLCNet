S3FNET_DIR = ../..
MODEL_NAME = test
VIRT_SPACE = 10.10.0.0

all:	$(MODEL_NAME)-env.dml $(MODEL_NAME)-rt.dml

$(MODEL_NAME)-env.dml:	$(MODEL_NAME).dml
	$(S3FNET_DIR)/dmlenv -b $(VIRT_SPACE) $(MODEL_NAME).dml > $@

$(MODEL_NAME)-rt.dml:   $(MODEL_NAME).dml
	$(S3FNET_DIR)/dmlenv -r all $(MODEL_NAME).dml > $@

test:   $(MODEL_NAME)-env.dml $(MODEL_NAME)-rt.dml
	$(S3FNET_DIR)/s3fnet *.dml

clean:
	rm -f $(MODEL_NAME)-env.dml $(MODEL_NAME)-rt.dml *~ .tmpdat*
