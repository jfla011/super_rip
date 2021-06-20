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


// TEST string too long
// TEST string too short
// TEST invalid characters

START_TEST(test_get_rip_network_valid_ipv4_net)
{
    rip_network_t rip_network = get_rip_network("172.25.0.0", 10);
    ck_assert_int_eq(rip_network.metric, 167772160); // Network byte order
    ck_assert_int_eq(rip_network.ip_address & 0x000000FF, 172);
    ck_assert_int_eq((rip_network.ip_address & 0x0000FF00) >> 8, 25);
}
END_TEST

START_TEST(test_get_rip_network_invalid_ipv4_net)
{
    rip_network_t rip_network = get_rip_network("invalid IP", 10);
}
END_TEST

START_TEST(test_parse_command)
{
    char *command_ref= "";
    char *bad_command = "bad command";
    ck_assert_int_eq(parse_command(bad_command, strlen(bad_command)), -1);
    char *command_test_1 = "ip rip add network 192.168.1.0\n";
    ck_assert_int_eq(parse_command(command_test_1, strlen(command_test_1)), 1);
    char *command_test_2 = "ip rip add network 203.123.213.0 5\n";
    ck_assert_int_eq(parse_command(command_test_2, strlen(command_test_2)), 1);
    char *command_test_3 = "ip rip add network 203.7890.789.7 5\n";
    ck_assert_int_eq(parse_command(command_test_3, strlen(command_test_3)), 2);
       
}
END_TEST

START_TEST(test_quit_command)
{
    char *command_test = "quit\n";
    parse_command(command_test, strlen(command_test));
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
    TCase *tc_rip_network;
    TCase *tc_command;

    s = suite_create("Super-Rip");

    /* Core test case */
    tc_core = tcase_create("Core");

    tcase_add_checked_fixture(tc_core, setup, teardown);
    tcase_add_test(tc_core, test_check_test_function);
    tcase_add_test(tc_core, test_1_equals_1);
    suite_add_tcase(s, tc_core);

    /* RIP Network test cases */
    tc_rip_network = tcase_create("Rip Network");

    tcase_add_test(tc_rip_network, test_get_rip_network_valid_ipv4_net);
    tcase_add_exit_test(tc_rip_network, test_get_rip_network_invalid_ipv4_net, 1);
    suite_add_tcase(s, tc_rip_network);

    /* CLI Related test cases */
    tc_command = tcase_create("Command");
    
    tcase_add_test(tc_command, test_parse_command);
    tcase_add_exit_test(tc_command, test_quit_command, 0);
    suite_add_tcase(s, tc_command);


    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = rip_suite();
    sr = srunner_create(s);

    srunner_set_log (sr, "test.log");
    srunner_run_all(sr, CK_VERBOSE);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
