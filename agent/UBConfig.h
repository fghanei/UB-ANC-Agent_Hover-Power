#ifndef UBCONFIG_H
#define UBCONFIG_H

#define STL_PORT    5760
#define NET_PORT    15760
#define PWR_PORT    35760
#define PXY_PORT    45760

#define AGENT_FILE      "agent"
#define FIRMWARE_FILE   "firmware"

#define MAV_DIR "mav_"
#define OBJ_DIR "objects"

#define PACKET_END      "\r\r\n\n"
#define BROADCAST_ID    255

#define SERIAL_PORT "ttyACM0"
#define BAUD_RATE   115200

#define POINT_ZONE      1
#define TAKEOFF_ALT     5
#define GPS_ACCURACY    5

#define MISSION_TRACK_RATE  1000

#define MAV_DATA_STREAM_RAW_SENSORS_RATE        0
#define MAV_DATA_STREAM_EXTENDED_STATUS_RATE    0
#define MAV_DATA_STREAM_RC_CHANNELS_RATE        0
#define MAV_DATA_STREAM_POSITION_RATE           5
#define MAV_DATA_STREAM_EXTRA1_RATE             10
#define MAV_DATA_STREAM_EXTRA2_RATE             0
#define MAV_DATA_STREAM_EXTRA3_RATE             0

#endif // UBCONFIG_H
