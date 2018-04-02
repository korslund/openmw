#include "configurationmanager.hpp"

#include <iostream>

#include <components/files/escape.hpp>

#include <boost/algorithm/string/erase.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <fstream>
/**
 * \namespace Files
 */
namespace Files
{

static const char* const openmwCfgFile = "openmw.cfg";

#if defined(_WIN32) || defined(__WINDOWS__)
static const char* const applicationName = "OpenMW";
#else
static const char* const applicationName = "openmw";
#endif

const char* const localToken = "?local?";
const char* const userDataToken = "?userdata?";
const char* const globalToken = "?global?";

ConfigurationManager::ConfigurationManager(bool silent)
    : mFixedPath(applicationName)
    , mSilent(silent)
{
    setupTokensMapping();

    sfs::create_directories(mFixedPath.getUserConfigPath());
    sfs::create_directories(mFixedPath.getUserDataPath());

    mLogPath = mFixedPath.getUserConfigPath();
}

ConfigurationManager::~ConfigurationManager()
{
}

void ConfigurationManager::setupTokensMapping()
{
    mTokensMapping.insert(std::make_pair(localToken, &FixedPath<>::getLocalPath));
    mTokensMapping.insert(std::make_pair(userDataToken, &FixedPath<>::getUserDataPath));
    mTokensMapping.insert(std::make_pair(globalToken, &FixedPath<>::getGlobalDataPath));
}

void ConfigurationManager::readConfiguration(boost::program_options::variables_map& variables,
    boost::program_options::options_description& description, bool quiet)
{
    bool silent = mSilent;
    mSilent = quiet;

    loadConfig(mFixedPath.getUserConfigPath(), variables, description);
    boost::program_options::notify(variables);

    // read either local or global config depending on type of installation
    bool loaded = loadConfig(mFixedPath.getLocalPath(), variables, description);
    boost::program_options::notify(variables);
    if (!loaded)
    {
        loadConfig(mFixedPath.getGlobalConfigPath(), variables, description);
        boost::program_options::notify(variables);
    }

    mSilent = silent;
}

void ConfigurationManager::processPaths(Files::PathContainer& dataDirs, bool create)
{
    std::string path;
    for (Files::PathContainer::iterator it = dataDirs.begin(); it != dataDirs.end(); ++it)
    {
        path = it->string();

        // Check if path contains a token
        if (!path.empty() && *path.begin() == '?')
        {
            std::string::size_type pos = path.find('?', 1);
            if (pos != std::string::npos && pos != 0)
            {
                TokensMappingContainer::iterator tokenIt = mTokensMapping.find(path.substr(0, pos + 1));
                if (tokenIt != mTokensMapping.end())
                {
                    sfs::path tempPath(((mFixedPath).*(tokenIt->second))());
                    if (pos < path.length() - 1)
                    {
                        // There is something after the token, so we should
                        // append it to the path
                        tempPath /= path.substr(pos + 1, path.length() - pos);
                    }

                    *it = tempPath;
                }
                else
                {
                    // Clean invalid / unknown token, it will be removed outside the loop
                    (*it).clear();
                }
            }
        }

        if (!sfs::is_directory(*it))
        {
            if (create)
            {
                try
                {
                    sfs::create_directories (*it);
                }
                catch (...) {}

                if (sfs::is_directory(*it))
                    continue;
            }

            (*it).clear();
        }
    }

    dataDirs.erase(std::remove_if(dataDirs.begin(), dataDirs.end(),
        std::bind(&sfs::path::empty, std::placeholders::_1)), dataDirs.end());
}

bool ConfigurationManager::loadConfig(const sfs::path& path,
    boost::program_options::variables_map& variables,
    boost::program_options::options_description& description)
{
    sfs::path cfgFile(path);
    cfgFile /= std::string(openmwCfgFile);
    if (sfs::is_regular_file(cfgFile))
    {
        if (!mSilent)
            std::cout << "Loading config file: " << cfgFile.string() << "... ";

        std::ifstream configFileStreamUnfiltered(cfgFile);
        boost::iostreams::filtering_istream configFileStream;
        configFileStream.push(escape_hash_filter());
        configFileStream.push(configFileStreamUnfiltered);
        if (configFileStreamUnfiltered.is_open())
        {
            boost::program_options::store(boost::program_options::parse_config_file(
                configFileStream, description, true), variables);

            if (!mSilent)
                std::cout << "done." << std::endl;
            return true;
        }
        else
        {
            if (!mSilent)
                std::cout << "failed." << std::endl;
            return false;
        }
    }
    return false;
}

const sfs::path& ConfigurationManager::getGlobalPath() const
{
    return mFixedPath.getGlobalConfigPath();
}

const sfs::path& ConfigurationManager::getUserConfigPath() const
{
    return mFixedPath.getUserConfigPath();
}

const sfs::path& ConfigurationManager::getUserDataPath() const
{
    return mFixedPath.getUserDataPath();
}

const sfs::path& ConfigurationManager::getLocalPath() const
{
    return mFixedPath.getLocalPath();
}

const sfs::path& ConfigurationManager::getGlobalDataPath() const
{
    return mFixedPath.getGlobalDataPath();
}

const sfs::path& ConfigurationManager::getCachePath() const
{
    return mFixedPath.getCachePath();
}

const sfs::path& ConfigurationManager::getInstallPath() const
{
    return mFixedPath.getInstallPath();
}

const sfs::path& ConfigurationManager::getLogPath() const
{
    return mLogPath;
}

} /* namespace Cfg */
