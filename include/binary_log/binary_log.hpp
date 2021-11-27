#pragma once
#include <set>
#include <string_view>

#include <binary_log/crc16.hpp>
#include <binary_log/fixed_string.hpp>
#include <binary_log/packer.hpp>
#include <binary_log/string_utils.hpp>

namespace binary_log
{
class binary_log
{
  std::FILE* m_index_file;
  std::FILE* m_log_file;

  // Format string table
  std::set<uint16_t> m_format_string_table;

  template<typename T>
  constexpr void pack_arg(const T& input)
  {
    packer::pack(m_log_file, input);
  }

  template<class T, class... Ts>
  constexpr void pack_args(T&& first, Ts&&... rest)
  {
    pack_arg(std::forward<T>(first));

    if constexpr (sizeof...(rest) > 0) {
      pack_args(std::forward<Ts>(rest)...);
    }
  }

  // template<typename T>
  // constexpr void pack_arg_type(const T& input)
  // {
  //   packer::pack_type(m_index_file, input);
  // }

  // template<class T, class... Ts>
  // constexpr void pack_arg_types(T&& first, Ts&&... rest)
  // {
  //   pack_arg_type(std::forward<T>(first));

  //   if constexpr (sizeof...(rest) > 0) {
  //     pack_arg_types(std::forward<Ts>(rest)...);
  //   }
  // }

public:
  binary_log(std::string_view path)
  {
    // Create the log file
    // All the log contents go here
    m_log_file = fopen(path.data(), "wb");
    if (m_log_file == nullptr) {
      throw std::invalid_argument("fopen failed");
    }

    // Create the index file
    std::string index_file_path = std::string {path} + ".index";
    m_index_file = fopen(index_file_path.c_str(), "wb");
    if (m_index_file == nullptr) {
      throw std::invalid_argument("fopen failed");
    }
  }

  ~binary_log() noexcept
  {
    fclose(m_log_file);
    fclose(m_index_file);
  }

  template<fixed_string F, uint16_t H, class... Args>
  constexpr inline void log(Args&&... args)
  {
    // Check if we need to update the index file
    // For a new format string, we need to update the index file
    constexpr char const* Name = F;

    auto it = m_format_string_table.find(H);
    uint8_t pos = 0; /* Assumption: There will be no more than 255 unique calls
                        on this logger */

    if (it == m_format_string_table.end()) {
      // SPEC:
      // <format-string-index [0-255]> <format-string-length> <format-string>
      // <number-of-arguments>

      auto result = m_format_string_table.insert(H);

      it = result.first;
      pos = std::distance(m_format_string_table.begin(), it);

      fwrite(&pos, sizeof(uint8_t), 1, m_index_file);

      // Write the length of the format string
      constexpr uint8_t format_string_length = string_length(Name);
      fwrite(&format_string_length, 1, 1, m_index_file);

      // Write the format string
      fwrite(F, 1, format_string_length, m_index_file);

      // Write the number of args taken by the format string
      constexpr uint8_t num_args = sizeof...(args);
      fwrite(&num_args, 1, 1, m_index_file);

    } else {
      pos = std::distance(m_format_string_table.begin(), it);
    }

    // Write to the main log file
    // SPEC:
    // <format-string-index> <arg1> <arg2> ... <argN>
    // <format-string-index> is the index of the format string in the index file
    // <arg1> <arg2> ... <argN> are the arguments to the format string
    //
    // Each <arg> is a pair: <type, value>

    // Write the format string index
    fwrite(&pos, sizeof(uint8_t), 1, m_log_file);

    // Write the args
    if constexpr (sizeof...(args) > 0) {
      pack_args(std::forward<Args>(args)...);
    }
  }
};

}  // namespace binary_log

#define BINARY_LOG(logger, format_string, ...) \
  constexpr uint16_t CONCAT(format_string_id, __LINE__) = crc16( \
      format_string __AT__, binary_log::string_length(format_string __AT__)); \
  logger.log<format_string, CONCAT(format_string_id, __LINE__)>(__VA_ARGS__);
