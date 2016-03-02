
#ifndef __S3FNETDEFINITIONS_H__
#define __S3FNETDEFINITIONS_H__

// This enables composite synchronization. see s3f.h
#define COMPOSITE_SYNC



#define LOGGING
#define DEFAULT_TDF 50
//#define TAP_DISABLED

#define NR_SERIAL_DEVS 5
#define KERN_BUF_SIZE 100
#define TX_BUF_SIZE 100
#define RX_BUF_SIZE 200
#define PATH_TO_S3FNETLXC  "/home/vignesh/Desktop/PLCs/awlsim-0.42/s3fnet-lxc"
#define PATH_TO_READER_DATA "/home/vignesh/Desktop/PLCs/awlsim-0.42/s3fnet-lxc/data"

//#define PROGRESS_FORCE

//#define LXC_INIT_DEBUG
//#define LXC_INDIVIDUAL_STATS
//#define ADVANCE_DEBUG

#ifdef PROGRESS_FORCE
#define PROGRESS_FLAG 1
#else
#define PROGRESS_FLAG 0
#endif



#endif /*__S3FNETDEFINITIONS_H__*/
