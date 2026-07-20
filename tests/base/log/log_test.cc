#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

#include "base/log/log.h"

namespace rpc {
namespace fs = std::filesystem;

class LogTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir_ = "./test_log_dir_" + std::to_string(std::hash<std::thread::id>{}(std::this_thread::get_id()));
        fs::create_directories(test_dir_);
    }

    void TearDown() override { fs::remove_all(test_dir_); }

    std::string test_dir_;
};

TEST_F(LogTest, InitAndIsOpen) {
    Log *log = Log::Instance();
    log->init(0, test_dir_.c_str(), ".log", 0);

    EXPECT_TRUE(log->IsOpen());
    EXPECT_EQ(log->GetLevel(), 0);
}

TEST_F(LogTest, SetLevel) {
    Log *log = Log::Instance();
    log->init(0, test_dir_.c_str(), ".log", 0);

    log->SetLevel(2);
    EXPECT_EQ(log->GetLevel(), 2);

    log->SetLevel(3);
    EXPECT_EQ(log->GetLevel(), 3);
}

TEST_F(LogTest, SyncWrite) {
    Log *log = Log::Instance();
    log->init(0, test_dir_.c_str(), ".log", 0);

    LOG_DEBUG("debug message");
    LOG_INFO("info message");
    LOG_WARN("warn message");
    LOG_ERROR("error message");

    log->flush();

    fs::path log_file = test_dir_ / fs::directory_iterator(test_dir_).operator*().path().filename();
    EXPECT_TRUE(fs::exists(log_file));

    std::ifstream file(log_file);
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    EXPECT_NE(content.find("debug"), std::string::npos);
    EXPECT_NE(content.find("info"), std::string::npos);
    EXPECT_NE(content.find("warn"), std::string::npos);
    EXPECT_NE(content.find("error"), std::string::npos);
}

TEST_F(LogTest, AsyncWrite) {
    Log *log = Log::Instance();
    log->init(0, test_dir_.c_str(), ".log", 1024);

    for (int i = 0; i < 100; ++i) {
        LOG_INFO("async log message %d", i);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    log->flush();

    fs::path log_file = test_dir_ / fs::directory_iterator(test_dir_).operator*().path().filename();
    EXPECT_TRUE(fs::exists(log_file));

    std::ifstream file(log_file);
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    EXPECT_NE(content.find("async log message 99"), std::string::npos);
}

TEST_F(LogTest, LogLevelFilter) {
    Log *log = Log::Instance();
    log->init(2, test_dir_.c_str(), ".log", 0);

    LOG_DEBUG("should not appear");
    LOG_INFO("should not appear");
    LOG_WARN("should appear");
    LOG_ERROR("should appear");

    log->flush();

    fs::path log_file = test_dir_ / fs::directory_iterator(test_dir_).operator*().path().filename();
    std::ifstream file(log_file);
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    EXPECT_EQ(content.find("debug"), std::string::npos);
    EXPECT_EQ(content.find("info"), std::string::npos);
    EXPECT_NE(content.find("warn"), std::string::npos);
    EXPECT_NE(content.find("error"), std::string::npos);
}

TEST_F(LogTest, FormatMessage) {
    Log *log = Log::Instance();
    log->init(0, test_dir_.c_str(), ".log", 0);

    int int_val = 42;
    double double_val = 3.14;
    const char *str_val = "test";

    LOG_INFO("int=%d, double=%.2f, str=%s", int_val, double_val, str_val);

    log->flush();

    fs::path log_file = test_dir_ / fs::directory_iterator(test_dir_).operator*().path().filename();
    std::ifstream file(log_file);
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    EXPECT_NE(content.find("int=42"), std::string::npos);
    EXPECT_NE(content.find("double=3.14"), std::string::npos);
    EXPECT_NE(content.find("str=test"), std::string::npos);
}

TEST_F(LogTest, Flush) {
    Log *log = Log::Instance();
    log->init(0, test_dir_.c_str(), ".log", 0);

    LOG_INFO("test flush");
    log->flush();

    fs::path log_file = test_dir_ / fs::directory_iterator(test_dir_).operator*().path().filename();
    std::ifstream file(log_file);
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    EXPECT_NE(content.find("test flush"), std::string::npos);
}

}  // namespace rpc

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}