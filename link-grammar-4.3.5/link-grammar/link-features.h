#ifndef LINK_FEATURES_H
#define LINK_FEATURES_H

#ifdef  __cplusplus
# define LINK_BEGIN_DECLS  extern "C" {
# define LINK_END_DECLS    }
#else
# define LINK_BEGIN_DECLS
# define LINK_END_DECLS
#endif

#ifndef link_public_api
# define link_public_api(x) x
#endif

#define LINK_MAJOR_VERSION 4
#define LINK_MINOR_VERSION 3
#define LINK_MICRO_VERSION 5

#define LINK_VERSION_STRING "4.3.5"

#endif
