/*
 *------------------------------------------------------------------
 * test_macro.h
 *
 * May 3, 2013, ptatrai
 *
 * Copyright (c) 2013-2013 by cisco Systems, Inc.
 * All rights reserved.
 *
 *------------------------------------------------------------------
 */

#ifndef TEST_MACRO_H_
#define TEST_MACRO_H_

#define STR(a) #a
#define TEST(a)                                                                \
    if (!(a)) {                                                                \
        printf("%s:%d Test '%s' failed!\n", __FILE__, __LINE__, STR(a));       \
        exit(1);                                                               \
    }

#endif /* TEST_MACRO_H_ */
