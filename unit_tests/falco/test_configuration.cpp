// SPDX-License-Identifier: Apache-2.0
/*
Copyright (C) 2023 The Falco Authors.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless ASSERTd by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#include <gtest/gtest.h>
#include <falco/configuration.h>

#ifdef _WIN32
#define SET_ENV_VAR(env_var_name, env_var_value) _putenv_s(env_var_name, env_var_value)
#else
#define SET_ENV_VAR(env_var_name, env_var_value) setenv(env_var_name, env_var_value, 1)
#endif

static std::string sample_yaml =
	"base_value:\n"
	"    id: 1\n"
	"    name: 'sample_name'\n"
	"    subvalue:\n"
	"      subvalue2:\n"
	"        boolean: true\n"
	"base_value_2:\n"
	"  sample_list:\n"
	"    - elem1\n"
	"    - elem2\n"
	"    - elem3\n";

TEST(Configuration, configuration_exceptions)
{
	yaml_helper conf;

	/* Broken YAML */
	std::string sample_broken_yaml = sample_yaml + " /  bad_symbol";
	EXPECT_ANY_THROW(conf.load_from_string(sample_broken_yaml));

	/* Right YAML */
	EXPECT_NO_THROW(conf.load_from_string(sample_yaml));
}

TEST(Configuration, configuration_reload)
{
	yaml_helper conf;

	/* Clear and reload config */
	conf.load_from_string(sample_yaml);
	ASSERT_TRUE(conf.is_defined("base_value"));
	conf.clear();
	ASSERT_FALSE(conf.is_defined("base_value"));
	conf.load_from_string(sample_yaml);
	ASSERT_TRUE(conf.is_defined("base_value"));
}

TEST(Configuration, read_yaml_fields)
{
	yaml_helper conf;
	conf.load_from_string(sample_yaml);

	/* is_defined */
	ASSERT_TRUE(conf.is_defined("base_value"));
	ASSERT_TRUE(conf.is_defined("base_value_2"));
	ASSERT_FALSE(conf.is_defined("unknown_base_value"));

	/* get some fields */
	ASSERT_EQ(conf.get_scalar<int>("base_value.id", -1), 1);
	ASSERT_STREQ(conf.get_scalar<std::string>("base_value.name", "none").c_str(), "sample_name");
	ASSERT_EQ(conf.get_scalar<bool>("base_value.subvalue.subvalue2.boolean", false), true);

	/* get list field elements */
	ASSERT_STREQ(conf.get_scalar<std::string>("base_value_2.sample_list[0]", "none").c_str(), "elem1");
	ASSERT_STREQ(conf.get_scalar<std::string>("base_value_2.sample_list[1]", "none").c_str(), "elem2");
	ASSERT_STREQ(conf.get_scalar<std::string>("base_value_2.sample_list[2]", "none").c_str(), "elem3");

	/* get sequence */
	std::vector<std::string> seq;
	conf.get_sequence(seq, "base_value_2.sample_list");
	ASSERT_EQ(seq.size(), 3);
	ASSERT_STREQ(seq[0].c_str(), "elem1");
	ASSERT_STREQ(seq[1].c_str(), "elem2");
	ASSERT_STREQ(seq[2].c_str(), "elem3");
}

TEST(Configuration, modify_yaml_fields)
{
	std::string key = "base_value.subvalue.subvalue2.boolean";
	yaml_helper conf;

    /* Get original value */
    conf.load_from_string(sample_yaml);
	ASSERT_EQ(conf.get_scalar<bool>(key, false), true);

    /* Modify the original value */
    conf.set_scalar<bool>(key, false);
	ASSERT_EQ(conf.get_scalar<bool>(key, true), false);

    /* Modify it again */
    conf.set_scalar<bool>(key, true);
	ASSERT_EQ(conf.get_scalar<bool>(key, false), true);
}

TEST(Configuration, configuration_environment_variables)
{
    // Set an environment variable for testing purposes
    std::string env_var_value = "envVarValue";
    std::string env_var_name = "ENV_VAR";
    std::string default_value = "default";
    SET_ENV_VAR(env_var_name.c_str(), env_var_value.c_str());
    yaml_helper conf;

    std::string sample_yaml =
        "base_value:\n"
        "    id: $ENV_VAR\n"
        "    name: '${ENV_VAR}'\n"
        "    string: my_string\n"
        "    invalid: $${ENV_VAR}\n"
		"    invalid_env: $$ENV_VAR\n"
        "    escaped: \"${ENV_VAR}\"\n"
        "    subvalue:\n"
        "        subvalue2:\n"
        "            boolean: ${UNSED_XX_X_X_VAR}\n"
        "base_value_2:\n"
        "    sample_list:\n"
        "        - ${ENV_VAR}\n"
        "        - ' ${ENV_VAR}'\n"
        "        - $UNSED_XX_X_X_VAR\n";
    conf.load_from_string(sample_yaml);

    /* Check if the base values are defined */
    ASSERT_TRUE(conf.is_defined("base_value"));
    ASSERT_TRUE(conf.is_defined("base_value_2"));
    ASSERT_FALSE(conf.is_defined("unknown_base_value"));

    /* Test fetching of a regular string without any environment variable */
    std::string base_value_string = conf.get_scalar<std::string>("base_value.string", default_value);
    ASSERT_EQ(base_value_string, "my_string");

    /* Test fetching of escaped environment variable format. Should return the string as-is after stripping the leading `$` */
    std::string base_value_invalid = conf.get_scalar<std::string>("base_value.invalid", default_value);
    ASSERT_EQ(base_value_invalid, "${ENV_VAR}");

	/* Test fetching of invalid escaped environment variable format. Should return the string as-is */
    std::string base_value_invalid_env = conf.get_scalar<std::string>("base_value.invalid_env", default_value);
    ASSERT_EQ(base_value_invalid_env, "$$ENV_VAR");

    /* Test fetching of strings that contain environment variables */
    std::string base_value_id = conf.get_scalar<std::string>("base_value.id", default_value);
    ASSERT_EQ(base_value_id, "$ENV_VAR"); // Does not follow the `${VAR}` format, so it should be treated as a regular string

    std::string base_value_name = conf.get_scalar<std::string>("base_value.name", default_value);
    ASSERT_EQ(base_value_name, env_var_value); // Proper environment variable format

    std::string base_value_escaped = conf.get_scalar<std::string>("base_value.escaped", default_value);
    ASSERT_EQ(base_value_escaped, env_var_value); // Environment variable within quotes

    /* Test fetching of an undefined environment variable. Expected to return the default value.*/
    std::string unknown_boolean = conf.get_scalar<std::string>("base_value.subvalue.subvalue2.boolean", default_value);
    ASSERT_EQ(unknown_boolean, default_value);

    /* Test fetching of environment variables from a list */
    std::string base_value_2_list_0 = conf.get_scalar<std::string>("base_value_2.sample_list[0]", default_value);
    ASSERT_EQ(base_value_2_list_0, env_var_value); // Proper environment variable format

    std::string base_value_2_list_1 = conf.get_scalar<std::string>("base_value_2.sample_list[1]", default_value);
    ASSERT_EQ(base_value_2_list_1, " ${ENV_VAR}"); // Environment variable preceded by a space, hence treated as a regular string

    std::string base_value_2_list_2 = conf.get_scalar<std::string>("base_value_2.sample_list[2]", default_value);
    ASSERT_EQ(base_value_2_list_2, "$UNSED_XX_X_X_VAR"); // Does not follow the `${VAR}` format, so should be treated as a regular string

    /* Clear the set environment variable after testing */
    SET_ENV_VAR(env_var_name.c_str(), env_var_value.c_str());
}

TEST(Configuration, configuration_webserver_ip)
{
    falco_configuration falco_config;

    std::vector<std::string> valid_addresses = {"127.0.0.1",
                                                "1.127.0.1",
                                                "1.1.127.1",
                                                "1.1.1.127",
                                                "::",
                                                "::1",
                                                "1200:0000:AB00:1234:0000:2552:7777:1313",
                                                "1200::AB00:1234:0000:2552:7777:1313",
                                                "1200:0000:AB00:1234::2552:7777:1313",
                                                "21DA:D3:0:2F3B:2AA:FF:FE28:9C5A",
                                                "FE80:0000:0000:0000:0202:B3FF:FE1E:8329",
                                                "0.0.0.0",
                                                "9.255.255.255",
                                                "11.0.0.0",
                                                "126.255.255.255",
                                                "129.0.0.0",
                                                "169.253.255.255",
                                                "169.255.0.0",
                                                "172.15.255.255",
                                                "172.32.0.0",
                                                "191.0.1.255",
                                                "192.88.98.255",
                                                "192.88.100.0",
                                                "192.167.255.255",
                                                "192.169.0.0",
                                                "198.17.255.255",
                                                "223.255.255.255"};

    for (const std::string &address: valid_addresses) {
        std::string option = "webserver.listen_address=";
        option.append(address);

        std::vector<std::string> cmdline_config_options;
        cmdline_config_options.push_back(option);

        EXPECT_NO_THROW(falco_config.init(cmdline_config_options));

        ASSERT_EQ(falco_config.m_webserver_listen_address, address);
    }

    std::vector<std::string> invalid_addresses = {"327.0.0.1",
                                                  "1.327.0.1",
                                                  "1.1.327.1",
                                                  "1.1.1.327",
                                                  "12 7.0.0.1",
                                                  "127. 0.0.1",
                                                  "127.0. 0.1",
                                                  "127.0.0. 1",
                                                  "!27.0.0.1",
                                                  "1200: 0000:AB00:1234:0000:2552:7777:1313",
                                                  "1200:0000: AB00:1234:0000:2552:7777:1313",
                                                  "1200:0000:AB00: 1234:0000:2552:7777:1313",
                                                  "1200:0000:AB00:1234: 0000:2552:7777:1313",
                                                  "1200:0000:AB00:1234:0000: 2552:7777:1313",
                                                  "1200:0000:AB00:1234:0000:2552: 7777:1313",
                                                  "1200:0000:AB00:1234:0000:2552:7777: 1313",
                                                  "1200:0000:AB00:1234:0000:2552:7777:131G",
                                                  "1200:0000:AB00:1234:0000:2552:77Z7:1313",
                                                  "1200:0000:AB00:1234:0000:2G52:7777:1313",
                                                  "1200:0000:AB00:1234:0O00:2552:7777:1313",
                                                  "1200:0000:AB00:H234:0000:2552:7777:1313",
                                                  "1200:0000:IB00:1234:0000:2552:7777:1313",
                                                  "1200:0O00:AB00:1234:0000:2552:7777:1313",
                                                  "12O0:0000:AB00:1234:0000:2552:7777:1313",};

    for (const std::string &address: invalid_addresses) {
        std::string option = "webserver.listen_address=";
        option.append(address);

        std::vector<std::string> cmdline_config_options;
        cmdline_config_options.push_back(option);

        EXPECT_ANY_THROW(falco_config.init(cmdline_config_options));
    }
}
