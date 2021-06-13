#include <stdlib.h>
#include <stdio.h>
#include <check.h>
#include "../src/super_rip.h"


void setup(void)
{
    printf("Setup function\n");
}

void teardown(void)
{
    printf("Teardown function\n");
}

START_TEST(test_1_equals_1)
{
    ck_assert_int_eq(1,1);
}
END_TEST

START_TEST(test_check_test_function)
{
    int value = 5;
    int result = check_test_function(value);

    ck_assert_int_eq(result, 6);
}
END_TEST

// START_TEST(test_money_create)
// {
//     ck_assert_int_eq(money_amount(five_dollars), 5);
//     ck_assert_str_eq(money_currency(five_dollars), "USD");
// }
// END_TEST
// 
// START_TEST(test_money_create_neg)
// {
//     Money *m = money_create(-1, "USD");
// 
//     ck_assert_msg(m == NULL,
//                   "NULL should be returned on attempt to create with "
//                   "a negative amount");
// }
// END_TEST
// 
// START_TEST(test_money_create_zero)
// {
//     Money *m = money_create(0, "USD");
// 
//     if (money_amount(m) != 0)
//     {
//         ck_abort_msg("Zero is a valid amount of money");
//     }
// }
// END_TEST

Suite * rip_suite(void)
{
    Suite *s;
    TCase *tc_core;
    TCase *tc_limits;

    s = suite_create("Super-Rip");

    /* Core test case */
    tc_core = tcase_create("Core");

    tcase_add_checked_fixture(tc_core, setup, teardown);
    tcase_add_test(tc_core, test_check_test_function);
    tcase_add_test(tc_core, test_1_equals_1);
    suite_add_tcase(s, tc_core);

//    /* Limits test case */
//    tc_limits = tcase_create("Limits");
//
//    tcase_add_test(tc_limits, test_money_create_neg);
//    tcase_add_test(tc_limits, test_money_create_zero);
//    suite_add_tcase(s, tc_limits);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = rip_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
