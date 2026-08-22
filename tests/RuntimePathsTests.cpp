#include "RuntimePathsTests.hpp"

#include "game/RuntimePaths.hpp"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <string>
#include <system_error>

namespace
{
    // 此运行器统一输出运行路径测试结果并累计失败数量。
    class RuntimePathTestRunner
    {
    public:
        // 此函数记录一个布尔断言的名称和结果。
        void check(const bool condition, const std::string& name)
        {
            if (condition)
            {
                std::cout << "[PASS] " << name << '\n';
                return;
            }
            std::cerr << "[FAIL] " << name << '\n';
            ++failureCount_;
        }

        // 此函数返回当前累计失败数量。
        int failureCount() const noexcept
        {
            return failureCount_;
        }

    private:
        int failureCount_ = 0;
    };
}

// 此函数使用临时目录验证发布路径优先级，不读取或修改正式 data。
int runRuntimePathTests()
{
    RuntimePathTestRunner runner;
    runner.check(
        std::filesystem::is_directory(
            autochess::game::executableDirectory()),
        "runtime path finds executable directory");

    const auto uniqueToken =
        std::chrono::steady_clock::now().time_since_epoch().count();
    const std::filesystem::path testRoot =
        std::filesystem::temp_directory_path()
        / ("autochess-runtime-path-tests-" + std::to_string(uniqueToken));
    const std::filesystem::path executableRoot = testRoot / "program";
    const std::filesystem::path adjacentData = executableRoot / "data";
    const std::filesystem::path developmentData =
        testRoot / "development-data";
    std::error_code error;

    // 此代码块建立彼此隔离的程序旁数据目录和开发回退目录。
    std::filesystem::create_directories(adjacentData, error);
    const bool adjacentCreated = !error;
    error.clear();
    std::filesystem::create_directories(developmentData, error);
    const bool developmentCreated = !error;
    runner.check(
        adjacentCreated && developmentCreated,
        "runtime path test directories are created");

    // 此代码块验证发布目录存在 data 时不会读取编译期开发目录。
    runner.check(
        autochess::game::selectDataDirectory(
            executableRoot,
            developmentData) == adjacentData,
        "runtime path prefers adjacent data directory");

    // 此代码块移除旁置 data 后验证开发环境仍可使用编译期目录。
    error.clear();
    std::filesystem::remove_all(adjacentData, error);
    runner.check(
        !error
            && autochess::game::selectDataDirectory(
                executableRoot,
                developmentData) == developmentData,
        "runtime path falls back to development data directory");

    // 此代码块仅清理本测试创建的唯一临时目录。
    error.clear();
    std::filesystem::remove_all(testRoot, error);
    runner.check(!error, "runtime path test directory is removed");
    return runner.failureCount();
}
