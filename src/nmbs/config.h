#ifndef __NMBS_CONFIG_H__
#define __NMBS_CONFIG_H__

/* This file specifies the configuration of the nanomodbus library
	 components. By default, this library has everything enabled (which
   isn't explicitly disabled). Because of this, the build size is a bit large.
   That's why this file was created - to disable everything and enable only
   what's needed in the current project. */

#define NMBS_CLIENT_DISABLED
// #define NMBS_SERVER_DISABLED

#define NMBS_SERVER_READ_COILS_DISABLED
#define NMBS_SERVER_READ_DISCRETE_INPUTS_DISABLED
// #define NMBS_SERVER_READ_HOLDING_REGISTERS_DISABLED
#define NMBS_SERVER_READ_INPUT_REGISTERS_DISABLED
#define NMBS_SERVER_WRITE_SINGLE_COIL_DISABLED
// #define NMBS_SERVER_WRITE_SINGLE_REGISTER_DISABLED
#define NMBS_SERVER_WRITE_MULTIPLE_COILS_DISABLED
// #define NMBS_SERVER_WRITE_MULTIPLE_REGISTERS_DISABLED
#define NMBS_SERVER_READ_FILE_RECORD_DISABLED
// #define NMBS_SERVER_WRITE_FILE_RECORD_DISABLED
#define NMBS_SERVER_READ_WRITE_REGISTERS_DISABLED
#define NMBS_SERVER_READ_DEVICE_IDENTIFICATION_DISABLED

#endif /* __NMBS_CONFIG_H__ */
