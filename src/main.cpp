#include <iostream>
#include <format>
#include <string>
#include <cstdlib>
#include <ranges>
#include <chrono>
#include <algorithm>
#include <numeric>
#include <list>
#include <unordered_map>
#include <unordered_set>
#include <filesystem>
#include <mutex>
#include <vector>
#include <thread>
#include <random>
#include <print>
#include <semaphore>

#include "libtorrent/hasher.hpp"
#include "libtorrent/alert.hpp"
#include "libtorrent/alert_types.hpp"
#include "libtorrent/entry.hpp"
#include "libtorrent/session.hpp"
#include "libtorrent/kademlia/dht_state.hpp"
#include "libtorrent/error_code.hpp"
#include "libtorrent/magnet_uri.hpp"

// 初始 DHT 接入节点
constexpr std::string_view dht_bootstrap_nodes[]{
    "dht.libtorrent.org:25401",
    "dht.transmissionbt.com:6881",
    "router.utorrent.com:6881",
    "router.silotis.us:6881",
    "dht.aelitis.com:6881",
    "dht.choking.chicken:6881",
    "router.bitcomet.com:6881",
    "dht.mopf.ru:6881",
};

// 默认缓存文件夹
constexpr std::string_view temp_cache_dir{
    "./temp"
};

// 同时加载的种子数最大值
constexpr std::size_t max_torrent_count{ 10ull };

// 种子加载超时时间（秒）
constexpr std::time_t max_torrent_time{ 60 };

template<>
struct std::formatter<lt::sha1_hash, char> {
private:
    bool uppercase = false;
public:
    constexpr auto parse(auto& context) {
        auto iter = std::begin(context);
        auto end = std::begin(context);
        if (iter == end) {
            return iter;
        }
        if (*iter == '}') {
            uppercase = false;
        }
        else if (*iter == 'x' || *iter == 'X') {
            uppercase = (*iter == 'X');
            if (*(++iter) != '}')
                throw std::format_error("libtorrent::sha1_hash format string.");
        }
        else {
            throw std::format_error("libtorrent::sha1_hash format string.");
        }
        return iter;
    }
    constexpr auto format(const lt::sha1_hash& hash, auto& fc) const {
        constexpr std::ptrdiff_t size = lt::sha1_hash::size();
        const char* data = hash.data();
        std::ptrdiff_t index;
        for (index = 0; index < size; ++index) {
            if (uppercase) {
                std::format_to(fc.out(), "{:02X}", static_cast<std::uint8_t>(data[index]));
            }
            else {
                std::format_to(fc.out(), "{:02x}", static_cast<std::uint8_t>(data[index]));
            }
        }
        return fc.out();
    }
};

namespace utils {

    inline void set_locale() {
        setlocale(LC_ALL, "zh_CN.UTF-8"); // C
        std::locale::global(std::locale("zh_CN.UTF-8")); // C++
    }

    std::uintmax_t clean_dir(const std::filesystem::path& dir) {
        return std::filesystem::remove_all(dir);
    }

    bool reset_dir(const std::filesystem::path& dir) {
        clean_dir(dir);
        return std::filesystem::create_directory(dir);
    }

    std::uint16_t random_port(std::uint16_t min, std::uint16_t max) {
        std::random_device device;
        std::mt19937_64 mt64 = std::mt19937_64(device());
        std::uniform_int_distribution<> dis(min, max); // 随机端口范围 [min, max]
        return dis(mt64);
    }
}

lt::settings_pack get_settings() {
    lt::settings_pack settings;
    settings.set_str(lt::settings_pack::user_agent, std::format("{}/{}", DHTX_PROJECT_NAME, DHTX_VERSION));
    settings.set_str(lt::settings_pack::listen_interfaces, std::format("0.0.0.0:{0},[::]:{0}", utils::random_port(11000, 19000)));
    std::string nodes = std::views::all(dht_bootstrap_nodes) | std::views::join_with(',') | std::ranges::to<std::string>();
    settings.set_str(lt::settings_pack::dht_bootstrap_nodes, nodes);

    lt::alert_category_t mask =
        lt::alert_category::port_mapping
        | lt::alert_category::dht
        //| lt::alert_category::port_mapping_log
        | lt::alert_category::status
        | lt::alert_category::storage
        | lt::alert_category::error;
    settings.set_int(lt::settings_pack::alert_mask, mask);
    return settings;
}

void alerts(std::binary_semaphore& signal) {
    lt::settings_pack settings = get_settings();
    lt::session session{ settings };

    std::unordered_set<std::string> hashs;
    std::unordered_set<lt::torrent_handle> handles;

    while (true) {
        if (signal.try_acquire()) { // 判断是否需要结束
            break;
        }
        std::vector<lt::alert*> alerts;
        session.pop_alerts(&alerts);
        std::string hash;
        for (lt::alert* alert : alerts) {
            if (lt::alert_cast<lt::dht_get_peers_alert>(alert)) {
                // dht 收到 peers 请求的 alert
                auto& gpa = *lt::alert_cast<lt::dht_get_peers_alert>(alert);
                std::string hash = std::format("{}", gpa.info_hash);
                if (hashs.find(hash) == hashs.end() && handles.size() < max_torrent_count) {
                    hashs.insert(hash);
                    lt::add_torrent_params torrent_params;
                    torrent_params.info_hashes = lt::info_hash_t{ gpa.info_hash };
                    torrent_params.flags |= lt::torrent_flags::upload_mode;
                    torrent_params.save_path = temp_cache_dir;
                    handles.emplace(session.add_torrent(torrent_params));
                }
            }
            else if (lt::alert_cast<lt::state_update_alert>(alert)) {
                auto& sua = *lt::alert_cast<lt::state_update_alert>(alert);
                std::time_t now = time(nullptr);
                for (const lt::torrent_status& status : sua.status) {
                    if (status.errc) {
                        if (handles.erase(status.handle)) {
                            std::println("错误-移除磁力: magnet:?xt=urn:btih:{}", status.info_hashes.get_best());
                            session.remove_torrent(status.handle);
                        }
                    }
                    if (status.state != lt::torrent_status::checking_resume_data
                        && status.state != lt::torrent_status::checking_files
                        && status.state != lt::torrent_status::downloading_metadata) {
                        if (handles.find(status.handle) != handles.end()) {
                            std::shared_ptr<const lt::torrent_info> info = status.torrent_file.lock();
                            const lt::info_hash_t& hashes = info->info_hashes();
                            std::println("------------------------------------------------------------------------------------");
                            std::println("名称：{}\n", info->name());
                            std::println("总大小：{} B\n", info->total_size());
                            std::println("文件个数：{}\n", info->num_files());
                            if (hashes.has_v1()) {
                                std::println("磁力[V1]：magnet:?xt=urn:btih:{}", hashes.get(lt::protocol_version::V1));
                            }
                            if (hashes.has_v2()) {
                                std::println("磁力[V2]：magnet:?xt=urn:btih:{}", hashes.get(lt::protocol_version::V2));
                            }
                            std::println("------------------------------------------------------------------------------------");
                            handles.erase(status.handle);
                            session.remove_torrent(status.handle);
                        }
                    }
                    else if (status.state == lt::torrent_status::downloading_metadata
                        && now - status.added_time > max_torrent_time) {
                        if (handles.erase(status.handle)) {
                            std::println("超时-移除磁力: magnet:?xt=urn:btih:{}", status.info_hashes.get_best());
                            session.remove_torrent(status.handle);
                        }
                    }
                }
            }
            else {
                //std::println("msg：{}", alert->message());
            }
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
        if (handles.size() > 0)
            session.post_torrent_updates();
    }
    session.abort();
}



int main(int argc, char* argv[]) {
    utils::set_locale(); // 解决输出 utf-8 编码问题

    utils::reset_dir(temp_cache_dir);

    std::binary_semaphore signal(0);

    std::thread alerts_thread = std::thread(alerts, std::ref(signal));

    char stop{ '\0' }; // 输入 's' 结束程序
    while (stop != 's') std::cin >> stop;

    signal.release(); // 通知结束

    alerts_thread.join();

    utils::clean_dir(temp_cache_dir);

    return 0;
}
