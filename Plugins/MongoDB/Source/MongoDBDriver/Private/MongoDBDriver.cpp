// 2025 Copyright Pandores Marketplace

#include "MongoDBDriver.h"

#define LOCTEXT_NAMESPACE "FMongoDBDriverModule"

THIRD_PARTY_INCLUDES_START
#	include "mongocxx/instance.hpp"
#	include "mongocxx/logger.hpp"
THIRD_PARTY_INCLUDES_END

#include "MongoDB/Pool.h"

#include <string_view>

DEFINE_LOG_CATEGORY(LogMongoDB);

class FMongoLogger : public mongocxx::logger
{
public:
    FMongoLogger() = default;

    virtual ~FMongoLogger() = default;

    virtual void operator()(mongocxx::log_level level,
        bsoncxx::v_noabi::stdx::string_view domain,
        bsoncxx::v_noabi::stdx::string_view message) noexcept override
    {
        const FString Domain  = ConvertStringView(domain);
        const FString Message = ConvertStringView(message);

        switch (level)
        {
        case mongocxx::log_level::k_critical: 
        case mongocxx::log_level::k_error: 
            UE_LOG(LogMongoDB, Error, TEXT("MongoDB error: (%s) %s."), *Domain, *Message);
            break;
        case mongocxx::log_level::k_warning: 
            UE_LOG(LogMongoDB, Warning, TEXT("MongoDB warning: (%s) %s."), *Domain, *Message);
            break;
        case mongocxx::log_level::k_info: 
        case mongocxx::log_level::k_message: 
            UE_LOG(LogMongoDB, Log, TEXT("MongoDB info: (%s) %s."), *Domain, *Message);
            break;
        case mongocxx::log_level::k_debug: 
        case mongocxx::log_level::k_trace:  
            UE_LOG(LogMongoDB, Verbose, TEXT("MongoDB debug: (%s) %s."), *Domain, *Message);
            break;
        }
    }

private:
    static FString ConvertStringView(bsoncxx::v_noabi::stdx::string_view StringView)
    {
        const  FUTF8ToTCHAR Converter(StringView.data(), StringView.size());
        return FString(Converter.Length(), Converter.Get());
    }
};

void FMongoDBDriverModule::StartupModule()
{
    std::unique_ptr<mongocxx::logger> Logger(new FMongoLogger());

    try
    {
	    static mongocxx::instance Inst{MoveTemp(Logger)};
    }

    catch (const std::exception& Exception)
    {
        UE_LOG(LogMongoDB, Error, TEXT("Failed to initialize the MongoDB driver. Reason: %s"),
            UTF8_TO_TCHAR(Exception.what()));
    }
}

void FMongoDBDriverModule::ShutdownModule()
{

}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FMongoDBDriverModule, MongoDBDriver)