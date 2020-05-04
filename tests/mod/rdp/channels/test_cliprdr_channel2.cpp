/*
    This program is free software; you can redistribute it and/or modify it
     under the terms of the GNU General Public License as published by the
     Free Software Foundation; either version 2 of the License, or (at your
     option) any later version.

    This program is distributed in the hope that it will be useful, but
     WITHOUT ANY WARRANTY; without even the implied warranty of
     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
     Public License for more details.

    You should have received a copy of the GNU General Public License along
     with this program; if not, write to the Free Software Foundation, Inc.,
     675 Mass Ave, Cambridge, MA 02139, USA.

    Product name: redemption, a FLOSS RDP proxy
    Copyright (C) Wallix 2015
    Author(s): Christophe Grosjean, Raphael Zhou
*/


#include "test_only/test_framework/redemption_unit_tests.hpp"
#include "test_only/test_framework/working_directory.hpp"
#include "test_only/test_framework/file.hpp"

#include "test_only/fake_stat.hpp"
#include "test_only/lcg_random.hpp"
#include "mod/rdp/channels/cliprdr_channel.hpp"
#include "core/RDP/clipboard.hpp"
#include "core/session_reactor.hpp"
#include "core/RDP/clipboard/format_list_serialize.hpp"
#include "utils/sugar/algostring.hpp"
#include "core/log_id.hpp"
#include "capture/fdx_capture.hpp"
#include "mod/file_validator_service.hpp"

#include "./test_channel.hpp"


namespace
{
    constexpr uint32_t first_last_flags
        = CHANNELS::CHANNEL_FLAG_FIRST
        | CHANNELS::CHANNEL_FLAG_LAST
        | CHANNELS::CHANNEL_FLAG_SHOW_PROTOCOL;

    constexpr uint32_t first_flags
        = CHANNELS::CHANNEL_FLAG_FIRST
        | CHANNELS::CHANNEL_FLAG_SHOW_PROTOCOL;

    constexpr uint32_t last_flags
        = CHANNELS::CHANNEL_FLAG_LAST
        | CHANNELS::CHANNEL_FLAG_SHOW_PROTOCOL;

    struct Msg
    {
        struct Av : ut::flagged_bytes_view
        {
            template<class... Ts>
            Av(Ts&&... xs)
            : ut::flagged_bytes_view{xs...}
            {}
        };

        struct ToMod
        {
            uint32_t total_length;
            uint32_t flags;
            Av av;
        };

        struct ToFront
        {
            uint32_t total_length;
            uint32_t flags;
            Av av;
        };

        struct ToValidator
        {
            Av av;
        };

        struct Log6
        {
            Av av;
        };

        struct Nothing {};
        struct Missing {};

        int type;
        union
        {
            Log6 log6;
            ToMod to_mod;
            ToFront to_front;
            ToValidator to_validator;
        };

        struct OutputData
        {
            std::size_t other_av_len = 0;
            std::size_t mismatch_position = 0;
            ut::PatternView pattern_view = ut::PatternView::deduced;
            bool show_mismatch_position = false;
        };
        mutable OutputData output_data;

        Msg(Log6 log6) : type(0), log6(log6) {}
        Msg(ToMod to_mod) : type(1), to_mod(to_mod) {}
        Msg(ToFront to_front) : type(2), to_front(to_front) {}
        Msg(ToValidator to_validator) : type(3), to_validator(to_validator) {}
        Msg(Nothing) : type(4) {}
        Msg(Missing) : type(5) {}

        bool operator == (Msg const& other) const
        {
            other.output_data.show_mismatch_position = true;

            auto check_av = [&](Av const& lhs, Av const& rhs){
                this->output_data.other_av_len = rhs.size();
                other.output_data.other_av_len = lhs.size();
                this->output_data.pattern_view = rhs.pattern;
                other.output_data.pattern_view = lhs.pattern;
                bool r = ut::compare_bytes(this->output_data.mismatch_position, lhs, rhs);
                other.output_data.mismatch_position = this->output_data.mismatch_position;
                return r;
            };

            if (this->type == other.type)
            {
                auto check_chann = [&](auto& lhs, auto& rhs){
                    return
                        check_av(lhs.av, rhs.av)
                     && lhs.total_length == rhs.total_length
                     && lhs.flags == rhs.flags
                     ;
                };

                switch (other.type)
                {
                    case 0: return check_av(this->log6.av, other.log6.av);
                    case 1: return check_chann(this->to_mod, other.to_mod);
                    case 2: return check_chann(this->to_front, other.to_front);
                    case 3: return check_av(this->to_validator.av, other.to_validator.av);
                }
            }

            auto get_av = [](Msg const& msg){
                switch (msg.type)
                {
                    case 0: return msg.log6.av;
                    case 1: return msg.to_mod.av;
                    case 2: return msg.to_front.av;
                    case 3: return msg.to_validator.av;
                }
                return Av{};
            };

            check_av(get_av(*this), get_av(other));

            return false;
        }

        friend std::ostream& operator<<(std::ostream& out, Msg const& msg)
        {
            char const* names[]{
                "Log6", "ToMod", "ToFront", "ToValidator", "Nothing", "Missing"
            };

            out << names[msg.type] << "{";

            auto const pos = msg.output_data.mismatch_position;
            std::size_t av_len = 0;

            auto put_av = [&](auto av){
                av_len = av.size();

                if (msg.output_data.other_av_len == av_len
                    && msg.output_data.mismatch_position == av_len
                ) {
                    out << "..._av";
                    return ;
                }

                if (av.pattern == ut::PatternView::deduced) {
                    av.pattern = msg.output_data.pattern_view;
                }
                ut::put_view(pos, out, av);
                out << "_av";
            };

            auto put_chann = [&](auto data){
                out << data.total_length;
                switch (data.flags)
                {
                    case first_last_flags: out << ", first_last_flags, "; break;
                    case first_flags: out << ", first_flags, "; break;
                    case last_flags: out << ", last_flags, "; break;
                    default: out << ", " << data.flags << ", "; break;
                }
                put_av(data.av);
            };

            switch (msg.type)
            {
                case 0: put_av(msg.log6.av); break;
                case 1: put_chann(msg.to_mod); break;
                case 2: put_chann(msg.to_front); break;
                case 3: put_av(msg.to_validator.av); break;
            }

            out << "}";

            if (msg.output_data.other_av_len != av_len
                && msg.output_data.show_mismatch_position
            ) {
                out << "]\nCollections size mismatch: "
                    << av_len << " != " << msg.output_data.other_av_len;
            }

            return out;
        }
    };

    inline auto ininit_msg_comparator_compare = +[](void*, Msg const&)
    {
        // maybe used with dtor of ClipboardVirtualChannel
        RED_REQUIRE(false);
        return true;
    };

    struct MsgComparator
    {
        using func_t = bool(*)(void*, Msg const&);

        struct ExceptionLock
        {
            MsgComparator& self;

            ~ExceptionLock()
            {
                if (std::uncaught_exceptions()) {
                    self.has_exception = true;
                }
            }
        };

        template<class Fn, class Process, class... Fns>
        void run(Fn fn, Process&& process, Fns... fns)
        {
            struct Tuple : Fn, Fns... {} t {fn, fns...};
            func_t table[]{
                func_t([](void* data, Msg const& msg){
                    return static_cast<Fn&>(*static_cast<Tuple*>(data))(msg);
                }),
                func_t([](void* data, Msg const& msg){
                    return static_cast<Fns&>(*static_cast<Tuple*>(data))(msg);
                })...
            };
            auto av_table = make_array_view(table);
            this->fn_table = av_table;
            this->index_table = 1;
            this->data = &t;

            ExceptionLock lock{*this};
            process();

            for (auto f : av_table.from_offset(this->index_table)) {
                f(&t, Msg::Nothing{});
            }

            this->fn_table = {&ininit_msg_comparator_compare, 1};
            this->index_table = 1;
        }

        array_view<func_t> fn_table {&ininit_msg_comparator_compare, 1};
        std::size_t index_table = 0;
        void* data;
        bool has_exception = false;

        void push(Msg const& msg)
        {
            if (this->has_exception) {
                return ;
            }

            while (1) {
                if (this->index_table < this->fn_table.size()) {
                    bool r = this->fn_table[this->index_table](this->data, msg);
                    ++this->index_table;
                    if (r) {
                        return;
                    }
                }
                else {
                    this->fn_table[0](this->data, msg);
                    return;
                }
            }
        }
    };

#define TEST_PROCESS TEST_BUF(Msg::Missing{}), [&]

#define TEST_BUF(...) [&](Msg const& msg_) { \
    [&](Msg const& expected) {               \
        RED_CHECK(expected == msg_);         \
    }(Msg(__VA_ARGS__));                     \
    return true;                             \
}

#define TEST_BUF_IF(cond, ...) [&](Msg const& msg_) { \
    if (cond) {                                       \
        RED_CHECK(Msg(__VA_ARGS__) == msg_);          \
        return true;                                  \
    }                                                 \
    return false;                                     \
}


    class FrontSenderTest : public VirtualChannelDataSender
    {
        MsgComparator& msg_comparator;

    public:
        explicit FrontSenderTest(MsgComparator& msg_comparator)
        : msg_comparator(msg_comparator)
        {}

        void operator()(uint32_t total_length, uint32_t flags, bytes_view chunk_data) override
        {
            msg_comparator.push(Msg::ToFront{total_length, flags, chunk_data});
        }
    };

    class ModSenderTest : public VirtualChannelDataSender
    {
        MsgComparator& msg_comparator;

    public:
        explicit ModSenderTest(MsgComparator& msg_comparator)
        : msg_comparator(msg_comparator)
        {}

        void operator()(uint32_t total_length, uint32_t flags, bytes_view chunk_data) override
        {
            msg_comparator.push(Msg::ToMod{total_length, flags, chunk_data});
        }
    };

    class ReportMessageTest : public NullReportMessage
    {
        MsgComparator& msg_comparator;

    public:
        ReportMessageTest(MsgComparator& msg_comparator)
        : msg_comparator(msg_comparator)
        {}

        void log6(LogId id, const timeval /*time*/, KVList kv_list) override
        {
            std::string s = detail::log_id_string_map[int(id)].data();
            for (auto& kv : kv_list) {
                str_append(s, ' ', kv.key, '=', kv.value);
            }
            this->msg_comparator.push(Msg::Log6{bytes_view(s)});
        }
    };

    class ValidatorTransportTest : public Transport
    {
        MsgComparator& msg_comparator;

    public:
        bytes_view buf_reader {};

        ValidatorTransportTest(MsgComparator& msg_comparator)
        : msg_comparator(msg_comparator)
        {}

        void do_send(const uint8_t * buffer, std::size_t len) override
        {
            this->msg_comparator.push(Msg::ToValidator{bytes_view(buffer, len)});
        }

        size_t do_partial_read(uint8_t* buffer, size_t len) override
        {
            len = std::min(len, this->buf_reader.size());
            memcpy(buffer, this->buf_reader.data(), len);
            this->buf_reader = this->buf_reader.from_offset(len);
            return len;
        }
    };

    struct Buffer
    {
        StaticOutStream<1600> out;

        Buffer() {}

        template<class F>
        bytes_view build(uint16_t msgType, uint16_t msgFlags, F f)
        {
            using namespace RDPECLIP;
            array_view_u8 av = out.out_skip_bytes(CliprdrHeader::size());
            f(this->out);
            OutStream stream_header(av);
            CliprdrHeader(msgType, msgFlags, uint32_t(out.get_offset() - av.size()))
                .emit(stream_header);
            return out.get_produced_bytes();
        }

        template<class F>
        bytes_view build_ok(uint16_t msgType, F f)
        {
            return this->build(msgType, RDPECLIP::CB_RESPONSE_OK, f);
        }
    };


    using namespace std::string_view_literals;

    inline constexpr auto fdx_basename = "sid,blabla.fdx"sv;
    inline constexpr auto sid = "my_session_id"sv;

    struct FdxTestCtx
    {
        FdxTestCtx(char const* name)
        : wd(name)
        {}

        WorkingDirectory wd;
        WorkingDirectory::SubDirectory record_path = wd.create_subdirectory("record");
        WorkingDirectory::SubDirectory hash_path = wd.create_subdirectory("hash");
        WorkingDirectory::SubDirectory fdx_record_path = record_path.create_subdirectory(sid);
        WorkingDirectory::SubDirectory fdx_hash_path = hash_path.create_subdirectory(sid);
        CryptoContext cctx;
        LCGRandom rnd;
        FakeFstat fstat;
        FdxCapture fdx = FdxCapture{
            record_path.dirname().string(),
            hash_path.dirname().string(),
            "sid,blabla", sid, -1, cctx, rnd, fstat,
            ReportError()};
    };

    auto add_file(FdxTestCtx& data_test, std::string_view suffix)
    {
        auto basename = str_concat(sid, suffix);
        auto path = data_test.fdx_record_path.add_file(basename);
        (void)data_test.fdx_hash_path.add_file(basename);
        return path;
    }


    struct ClipDataTest
    {
        bool with_validator;
        bool with_fdx_capture;
        bool always_file_storage;
        bool verify_before_download;

        friend std::ostream& operator<<(std::ostream& out, ClipDataTest const& d)
        {
            return out <<
                "with validator: " << d.with_validator <<
                "  with fdx: " << d.with_fdx_capture <<
                "  always_file_storage: " << d.always_file_storage <<
                "  verify_before_download: " << d.verify_before_download
            ;
        }

        ClipboardVirtualChannelParams default_channel_params() const
        {
            ClipboardVirtualChannelParams clipboard_virtual_channel_params {};
            clipboard_virtual_channel_params.clipboard_down_authorized = true;
            clipboard_virtual_channel_params.clipboard_up_authorized   = true;
            clipboard_virtual_channel_params.clipboard_file_authorized = true;
            clipboard_virtual_channel_params.validator_params.down_target_name = "down";
            clipboard_virtual_channel_params.validator_params.up_target_name = "up";
            clipboard_virtual_channel_params.validator_params.log_if_accepted = true;
            clipboard_virtual_channel_params.validator_params.enable_clipboard_text_up = true;
            clipboard_virtual_channel_params.validator_params.enable_clipboard_text_down = true;
            clipboard_virtual_channel_params.validator_params.verify_before_download
                = this->verify_before_download;
            return clipboard_virtual_channel_params;
        }

        std::unique_ptr<FdxTestCtx> make_optional_fdx_ctx() const
        {
            if (this->with_fdx_capture) {
                return std::make_unique<FdxTestCtx>(
                    &"abcdefgh"[this->with_validator * 2 + this->always_file_storage]
                );
            }
            return std::unique_ptr<FdxTestCtx>();
        }

        class ChannelCtx
        {
            SessionReactor session_reactor;

            ReportMessageTest report_message;
            ValidatorTransportTest validator_transport;
            FileValidatorService file_validator_service{validator_transport};

            FrontSenderTest to_client_sender;
            ModSenderTest to_server_sender;

            ClipboardVirtualChannel clipboard_virtual_channel;

        public:
            ChannelCtx(
                MsgComparator& msg_comparator,
                FdxTestCtx* fdx_ctx,
                ClipboardVirtualChannelParams clipboard_virtual_channel_params,
                ClipDataTest const& d, RDPVerbose verbose)
            : report_message(msg_comparator)
            , validator_transport(msg_comparator)
            , file_validator_service(validator_transport)
            , to_client_sender(msg_comparator)
            , to_server_sender(msg_comparator)
            , clipboard_virtual_channel(
                &to_client_sender, &to_server_sender, session_reactor,
                BaseVirtualChannel::Params(report_message, verbose),
                clipboard_virtual_channel_params,
                d.with_validator ? &file_validator_service : nullptr,
                ClipboardVirtualChannel::FileStorage{
                    fdx_ctx ? &fdx_ctx->fdx : nullptr,
                    d.always_file_storage
                }
            )
            {}

            using ValidationResult = LocalFileValidatorProtocol::ValidationResult;

            bool dlp_message_accept(FileValidatorId id)
            {
                return this->dlp_message(ValidationResult::IsAccepted, id);
            }

            bool dlp_message_reject(FileValidatorId id)
            {
                return this->dlp_message(ValidationResult::IsRejected, id);
            }

            bool dlp_message(ValidationResult result, FileValidatorId id)
            {
                StaticOutStream<256> out;
                auto status = (result == ValidationResult::IsAccepted)
                    ? "ok"_av
                    : "fail"_av;

                using namespace LocalFileValidatorProtocol;
                FileValidatorHeader(MsgType::Result, 0/*len*/).emit(out);
                FileValidatorResultHeader{ValidationResult::IsAccepted, id,
                    checked_int(status.size())}.emit(out);
                out.out_copy_bytes(status);

                validator_transport.buf_reader = out.get_produced_bytes();
                clipboard_virtual_channel.DLP_antivirus_check_channels_files();
                return validator_transport.buf_reader.size() == 0;
            }

        private:
            std::unique_ptr<AsynchronousTask> out_asynchronous_task;
            Inifile ini;
            SesmanInterface sesman{ini};

        public:
            void process_server_message(bytes_view av)
            {
                auto flags
                    = CHANNELS::CHANNEL_FLAG_FIRST
                    | CHANNELS::CHANNEL_FLAG_LAST
                    | CHANNELS::CHANNEL_FLAG_SHOW_PROTOCOL;
                clipboard_virtual_channel.process_server_message(
                    av.size(), flags, av, out_asynchronous_task, sesman);
                // RED_TEST(!this->out_asynchronous_task);
            }

            void process_client_message(bytes_view av, int flags
                = CHANNELS::CHANNEL_FLAG_FIRST
                | CHANNELS::CHANNEL_FLAG_LAST)
            {
                flags |= CHANNELS::CHANNEL_FLAG_SHOW_PROTOCOL;
                clipboard_virtual_channel.process_client_message(av.size(), flags, av);
            }
        };
    };

    constexpr auto use_long_format = Cliprdr::IsLongFormat(true);
    constexpr auto file_group = Cliprdr::formats::file_group_descriptor_w.ascii_name;
    constexpr uint32_t file_group_id = 49262;
}

#define RED_AUTO_TEST_CLIPRDR(test_name, type_value, iocontext, ...) \
    void test_name##__case(type_value);                              \
    RED_AUTO_TEST_CASE(test_name) {                                  \
        auto l = __VA_ARGS__;                                        \
        auto p = l.begin();                                          \
        RED_TEST_CONTEXT_DATA(type_value, iocontext, l)              \
        {                                                            \
            auto const err_count = RED_ERROR_COUNT();                \
            test_name##__case(*p);                                   \
            if (err_count != RED_ERROR_COUNT()) {                    \
                break;                                               \
            };                                                       \
            ++p;                                                     \
        }                                                            \
    }                                                                \
    void test_name##__case(type_value)

using D = ClipDataTest;

RED_AUTO_TEST_CLIPRDR(TestCliprdrChannelFilterDataFileWithoutLock, D const& d, d, {
    //icap  fdx  storage verify
    D{true, false, true, false},
    D{true, true, false, false},
    D{true, true, true, false},

    D{false, false, true, false},
    D{false, true, false, false},
    D{false, true, true, false}
}) {
    bytes_view temp_av;
    MsgComparator msg_comparator;
    auto fdx_ctx = d.make_optional_fdx_ctx();
    auto channel_ctx = std::make_unique<D::ChannelCtx>(
        msg_comparator,
        fdx_ctx.get(),
        d.default_channel_params(),
        d, RDPVerbose::cliprdr /*| RDPVerbose::cliprdr_dump*/);

    {
        using namespace RDPECLIP;
        Buffer buf;
        temp_av = buf.build(CB_CLIP_CAPS, 0, [&](OutStream& out){
            out.out_uint16_le(CB_CAPSTYPE_GENERAL);
            out.out_clear_bytes(2);
            GeneralCapabilitySet{CB_CAPS_VERSION_1, CB_USE_LONG_FORMAT_NAMES}.emit(out);
        });

        msg_comparator.run(
            TEST_PROCESS { channel_ctx->process_server_message(temp_av); },
            TEST_BUF(Msg::ToFront{24, first_last_flags, temp_av})
        );

        msg_comparator.run(
            TEST_PROCESS { channel_ctx->process_client_message(temp_av); },
            TEST_BUF(Msg::ToMod{24, first_last_flags, temp_av})
        );
    }

    msg_comparator.run(
        TEST_PROCESS {
            StaticOutStream<1600> out;
            Cliprdr::format_list_serialize_with_header(
                out,
                use_long_format,
                std::array{Cliprdr::FormatNameRef{file_group_id, file_group}});
            temp_av = out.get_produced_bytes();
            channel_ctx->process_client_message(temp_av);
        },
        TEST_BUF(Msg::ToMod{54, first_last_flags, temp_av})
    );

    // skip format list response

    msg_comparator.run(
        TEST_PROCESS {
            Buffer buf;
            temp_av = buf.build(RDPECLIP::CB_FORMAT_DATA_REQUEST, 0, [&](OutStream& out){
                out.out_uint32_le(file_group_id);
            });
            channel_ctx->process_server_message(temp_av);
        },
        TEST_BUF(Msg::ToFront{12, first_last_flags, temp_av})
    );

    msg_comparator.run(
        TEST_PROCESS {
            using namespace Cliprdr;
            Buffer buf;
            temp_av = buf.build_ok(RDPECLIP::CB_FORMAT_DATA_RESPONSE, [&](OutStream& out){
                out.out_uint32_le(1);    // count item
                out.out_uint32_le(RDPECLIP::FD_FILESIZE);    // flags
                out.out_clear_bytes(32); // reserved1
                out.out_uint32_le(0);    // attributes
                out.out_clear_bytes(16); // reserved2
                out.out_uint64_le(0);    // lastWriteTime
                out.out_uint32_le(0);    // file size high
                out.out_uint32_le(12);   // file size low
                auto filename = "abc"_utf16_le;
                out.out_copy_bytes(filename);
                out.out_clear_bytes(520u - filename.size());
            });
            channel_ctx->process_client_message(temp_av);
        },
        TEST_BUF(Msg::Log6{
            "CB_COPYING_PASTING_DATA_TO_REMOTE_SESSION"
            " format=FileGroupDescriptorW(49262) size=596"_av}),
        TEST_BUF(Msg::ToMod{604, first_last_flags, temp_av})
    );

    msg_comparator.run(
        TEST_PROCESS {
            using namespace RDPECLIP;
            Buffer buf;
            temp_av = buf.build_ok(CB_FILECONTENTS_REQUEST, [&](OutStream& out){
                FileContentsRequestPDU(0, 0, FILECONTENTS_RANGE, 0, 0, 12, 0, true).emit(out);
            });
            channel_ctx->process_server_message(temp_av);
        },
        TEST_BUF(Msg::ToFront{36, first_last_flags, temp_av})
    );

    // file content

    msg_comparator.run(
        TEST_PROCESS {
            using namespace Cliprdr;
            Buffer buf;
            temp_av = buf.build_ok(RDPECLIP::CB_FILECONTENTS_RESPONSE, [&](OutStream& out){
                out.out_uint32_le(0);
                out.out_copy_bytes("data_abcdefg"_av);
            });
            channel_ctx->process_client_message(temp_av);
        },
        TEST_BUF_IF(d.with_validator, Msg::ToValidator{
            "\x07\x00\x00\x00\x19\x00\x00\x00\x01\x00\x02up"
            "\x00\x01\x00\bfilename\x00\x03""abc"_av}),
        TEST_BUF_IF(d.with_validator, Msg::ToValidator{
            "\x01\x00\x00\x00\x10\x00\x00\x00\x01"_av}),
        TEST_BUF_IF(d.with_validator, Msg::ToValidator{"data_abcdefg"_av}),
        TEST_BUF(Msg::Log6{
            "CB_COPYING_PASTING_FILE_TO_REMOTE_SESSION"
            " file_name=abc size=12 sha256="
            "d1b9c9db455c70b7c6a70225a00f859931e498f7f5e07f2c962e1078c0359f5e"_av}),
        TEST_BUF_IF(d.with_validator, Msg::ToValidator{
            "\x03\x00\x00\x00\x04\x00\x00\x00\x01"_av}),
        TEST_BUF_IF(!d.verify_before_download, Msg::ToMod{24, first_last_flags, temp_av}),
        // without "data_abcdefg"
        TEST_BUF_IF(d.verify_before_download, Msg::ToMod{24, first_flags, temp_av.first(12)})
    );

    if (d.with_validator) {
        msg_comparator.run(
            TEST_PROCESS { RED_TEST(channel_ctx->dlp_message_accept(FileValidatorId(1))); },
            TEST_BUF(Msg::Log6{
                "FILE_VERIFICATION direction=UP file_name=abc status=ok"_av}),
            TEST_BUF_IF(d.verify_before_download, Msg::ToMod{24, last_flags, "data_abcdefg"_av})
        );
    }

    if (fdx_ctx && d.always_file_storage) {
        RED_CHECK_FILE_CONTENTS(add_file(*fdx_ctx, ",000001.tfl"), "data_abcdefg"sv);
    }

    // check TEXT_VALIDATION

    msg_comparator.run(
        TEST_PROCESS {
            StaticOutStream<1600> out;
            Cliprdr::format_list_serialize_with_header(
                out,
                Cliprdr::IsLongFormat(use_long_format),
                std::array{Cliprdr::FormatNameRef{RDPECLIP::CF_TEXT, nullptr}});
            temp_av = out.get_produced_bytes();
            channel_ctx->process_client_message(temp_av);
        },
        TEST_BUF(Msg::ToMod{14, first_last_flags, temp_av})
    );

    // skip format list response

    msg_comparator.run(
        TEST_PROCESS {
            Buffer buf;
            temp_av = buf.build(RDPECLIP::CB_FORMAT_DATA_REQUEST, 0, [&](OutStream& out){
                out.out_uint32_le(RDPECLIP::CF_UNICODETEXT);
            });
            channel_ctx->process_server_message(temp_av);
        },
        TEST_BUF(Msg::ToFront{12, first_last_flags, temp_av})
    );

    msg_comparator.run(
        TEST_PROCESS {
            using namespace Cliprdr;
            Buffer buf;
            temp_av = buf.build_ok(RDPECLIP::CB_FORMAT_DATA_RESPONSE, [&](OutStream& out){
                out.out_copy_bytes("a\0b\0c\0"_av);
            });
            channel_ctx->process_client_message(temp_av);
        },
        TEST_BUF_IF(d.with_validator, Msg::ToValidator{
            "\x07\x00\x00\x00\"\x00\x00\x00\x02\x00\x02up"
            "\x00\x01\x00\x13microsoft_locale_id\x00\x01""0"_av
        }),
        TEST_BUF(Msg::Log6{
            "CB_COPYING_PASTING_DATA_TO_REMOTE_SESSION_EX"
            " format=CF_UNICODETEXT(13) size=6 partial_data=abc"_av
        }),
        TEST_BUF_IF(d.with_validator, Msg::ToValidator{
            "\x01\x00\x00\x00\x07\x00\x00\x00\x02"_av
        }),
        TEST_BUF_IF(d.with_validator, Msg::ToValidator{"abc"_av}),
        TEST_BUF_IF(d.with_validator, Msg::ToValidator{
            "\x03\x00\x00\x00\x04\x00\x00\x00\x02"_av
        }),
        TEST_BUF(Msg::ToMod{14, first_last_flags, temp_av})
    );

    if (d.with_validator) {
        msg_comparator.run(
            TEST_PROCESS { RED_TEST(channel_ctx->dlp_message_accept(FileValidatorId(2))); },
            TEST_BUF(Msg::Log6{
                "TEXT_VERIFICATION direction=UP copy_id=2 status=ok"_av})
        );
    }

    msg_comparator.run(TEST_PROCESS {
        channel_ctx.reset();
    });

    if (fdx_ctx) {
        (void)fdx_ctx->hash_path.add_file(fdx_basename);
        auto fdx_path = fdx_ctx->record_path.add_file(fdx_basename);

        OutCryptoTransport::HashArray qhash;
        OutCryptoTransport::HashArray fhash;
        fdx_ctx->fdx.close(qhash, fhash);

        RED_CHECK_WORKSPACE(fdx_ctx->wd);

        if (d.always_file_storage) {
            RED_CHECK_FILE_CONTENTS(fdx_path,
                "v3\n\x04\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
                "\x00\x00,\x03\x00\x26\x00""abcmy_session_id/my_session_id,000001.tfl\xd1\xb9\xc9"
                "\xdb""E\\p\xb7\xc6\xa7\x02%\xa0\x0f\x85\x99""1\xe4\x98\xf7\xf5\xe0"
                "\x7f,\x96.\x10x\xc0""5\x9f^"_av);
        }
        else {
            RED_CHECK_FILE_CONTENTS(fdx_path, "v3\n"_av);
        }
    }
}

RED_AUTO_TEST_CLIPRDR(TestCliprdrBlockWithoutLock, D const& d, d, {
    //icap  fdx  storage verify
    D{true, false, true, true},
    D{true, true, false, true},
    D{true, true, true, true},
}) {
    RED_TEST(d.with_validator);
    RED_TEST(d.verify_before_download);

    bytes_view temp_av;
    MsgComparator msg_comparator;
    auto fdx_ctx = d.make_optional_fdx_ctx();
    auto channel_ctx = std::make_unique<D::ChannelCtx>(
        msg_comparator,
        fdx_ctx.get(),
        d.default_channel_params(),
        d, RDPVerbose::cliprdr /*| RDPVerbose::cliprdr_dump*/);

    {
        using namespace RDPECLIP;
        Buffer buf;
        temp_av = buf.build(CB_CLIP_CAPS, 0, [&](OutStream& out){
            out.out_uint16_le(CB_CAPSTYPE_GENERAL);
            out.out_clear_bytes(2);
            GeneralCapabilitySet{CB_CAPS_VERSION_1, CB_USE_LONG_FORMAT_NAMES}.emit(out);
        });

        msg_comparator.run(
            TEST_PROCESS { channel_ctx->process_server_message(temp_av); },
            TEST_BUF(Msg::ToFront{24, first_last_flags, temp_av})
        );

        msg_comparator.run(
            TEST_PROCESS { channel_ctx->process_client_message(temp_av); },
            TEST_BUF(Msg::ToMod{24, first_last_flags, temp_av})
        );
    }

    msg_comparator.run(
        TEST_PROCESS {
            StaticOutStream<1600> out;
            Cliprdr::format_list_serialize_with_header(
                out,
                use_long_format,
                std::array{Cliprdr::FormatNameRef{file_group_id, file_group}});
            temp_av = out.get_produced_bytes();
            channel_ctx->process_server_message(temp_av);
        },
        TEST_BUF(Msg::ToFront{54, first_last_flags, temp_av})
    );

    // skip format list response

    struct RequestedRange
    {
        enum class StreamId : uint32_t;

        MsgComparator& msg_comparator;
        ClipDataTest::ChannelCtx& channel_ctx;
        bytes_view file_contents {};

        Buffer filecontens_request_buf {};
        bytes_view filecontens_request_av {};

        // data_request + data_response of 1 file
        void prepare_file(bytes_view file_contents)
        {
            this->file_contents = file_contents;

            bytes_view av;
            msg_comparator.run(
                TEST_PROCESS {
                    Buffer buf;
                    av = buf.build(RDPECLIP::CB_FORMAT_DATA_REQUEST, 0, [&](OutStream& out){
                        out.out_uint32_le(file_group_id);
                    });
                    channel_ctx.process_client_message(av);
                },
                TEST_BUF(Msg::ToMod{12, first_last_flags, av})
            );

            msg_comparator.run(
                TEST_PROCESS {
                    using namespace Cliprdr;
                    Buffer buf;
                    av = buf.build_ok(RDPECLIP::CB_FORMAT_DATA_RESPONSE, [&](OutStream& out){
                        out.out_uint32_le(1);    // count item
                        out.out_uint32_le(RDPECLIP::FD_FILESIZE);    // flags
                        out.out_clear_bytes(32); // reserved1
                        out.out_uint32_le(0);    // attributes
                        out.out_clear_bytes(16); // reserved2
                        out.out_uint64_le(0);    // lastWriteTime
                        out.out_uint32_le(0);    // file size high
                        out.out_uint32_le(checked_int(file_contents.size())); // file size low
                        auto filename = "abc"_utf16_le;
                        out.out_copy_bytes(filename);
                        out.out_clear_bytes(520u - filename.size());
                    });
                    channel_ctx.process_server_message(av);
                },
                TEST_BUF(Msg::Log6{
                    "CB_COPYING_PASTING_DATA_FROM_REMOTE_SESSION"
                    " format=FileGroupDescriptorW(49262) size=596"_av}),
                TEST_BUF(Msg::ToFront{604, first_last_flags, av})
            );
        }

        void client_full_file_request(StreamId stream_id = StreamId(0))
        {
            this->client_file_request(0, this->file_contents.size(), stream_id);
        }

        void client_file_request(
            size_t offset, size_t len, StreamId stream_id = StreamId(0))
        {
            using namespace RDPECLIP;

            this->filecontens_request_buf.out.rewind();
            this->filecontens_request_av = this->filecontens_request_buf
            .build_ok(CB_FILECONTENTS_REQUEST, [&](OutStream& out){
                FileContentsRequestPDU(
                     safe_int(stream_id), 0, FILECONTENTS_RANGE,
                     checked_int(offset), 0, checked_int(len), 0, true)
                .emit(out);
            });

            this->channel_ctx.process_client_message(this->filecontens_request_av);
        }
    };

    using StreamId = RequestedRange::StreamId;
    RequestedRange requested{msg_comparator, *channel_ctx};

    requested.prepare_file("data_abcdefg"_av);

    /// full file with a packet.size() < 1600
    // TODO loop with validation = true | false
    //@{
    msg_comparator.run(
        TEST_PROCESS { requested.client_full_file_request(); },
        TEST_BUF(Msg::ToMod{36, first_last_flags, requested.filecontens_request_av})
    );

    msg_comparator.run(
        TEST_PROCESS {
            using namespace Cliprdr;
            Buffer buf;
            temp_av = buf.build_ok(RDPECLIP::CB_FILECONTENTS_RESPONSE, [&](OutStream& out){
                out.out_uint32_le(0);
                out.out_copy_bytes(requested.file_contents);
            });
            channel_ctx->process_server_message(temp_av);
        },
        TEST_BUF(Msg::ToValidator{
            "\x07\x00\x00\x00\x1b\x00\x00\x00\x01\x00\x04""down"
            "\x00\x01\x00\bfilename\x00\x03""abc"_av}),
        TEST_BUF(Msg::ToValidator{"\x01\x00\x00\x00\x10\x00\x00\x00\x01"_av}),
        TEST_BUF(Msg::ToValidator{requested.file_contents}),
        TEST_BUF(Msg::Log6{
            "CB_COPYING_PASTING_FILE_FROM_REMOTE_SESSION"
            " file_name=abc size=12 sha256="
            "d1b9c9db455c70b7c6a70225a00f859931e498f7f5e07f2c962e1078c0359f5e"_av}),
        TEST_BUF(Msg::ToValidator{"\x03\x00\x00\x00\x04\x00\x00\x00\x01"_av}),
        // without file_contents
        TEST_BUF(Msg::ToFront{24, first_flags, temp_av.first(12)})
    );

    msg_comparator.run(
        TEST_PROCESS { RED_TEST(channel_ctx->dlp_message_accept(FileValidatorId(1))); },
        TEST_BUF(Msg::Log6{"FILE_VERIFICATION direction=DOWN file_name=abc status=ok"_av}),
        TEST_BUF(Msg::ToFront{24, last_flags, requested.file_contents})
    );

    if (fdx_ctx && d.always_file_storage) {
        RED_CHECK_FILE_CONTENTS(add_file(*fdx_ctx, ",000001.tfl"), requested.file_contents);
    }
    //@}

    // data already available
    // TODO loop with validation = true | false
    //@{
    msg_comparator.run(
        TEST_PROCESS {
            requested.client_file_request(0, 6, StreamId(1));
        },
        TEST_BUF(Msg::ToFront{18, first_last_flags,
             "\x09\x00\x01\x00\x0a\x00\x00\x00\x01\x00\x00\x00""data_a"_av})
    );

    // big request
    msg_comparator.run(
        TEST_PROCESS {
            requested.client_file_request(5, 10000, StreamId(1));
        },
        TEST_BUF(Msg::ToFront{19, first_last_flags,
             "\x09\x00\x01\x00\x0b\x00\x00\x00\x01\x00\x00\x00""abcdefg"_av})
    );
    //@}

    requested.prepare_file("data_abcdefgf"_av);

    // full file with 2 requested ranges
    // TODO loop with validation = true | false
    //@{
    msg_comparator.run(
        TEST_PROCESS {
            requested.client_file_request(0, 7, StreamId(2));
        },
        TEST_BUF(Msg::ToMod{36, first_last_flags, requested.filecontens_request_av})
    );

    msg_comparator.run(
        TEST_PROCESS {
            using namespace Cliprdr;
            Buffer buf;
            temp_av = buf.build_ok(RDPECLIP::CB_FILECONTENTS_RESPONSE, [&](OutStream& out){
                out.out_uint32_le(2);
                out.out_copy_bytes(requested.file_contents.first(7));
            });
            channel_ctx->process_server_message(temp_av);
        },
        TEST_BUF(Msg::ToValidator{
            "\x07\x00\x00\x00\x1b\x00\x00\x00\x02\x00\x04""down"
            "\x00\x01\x00\bfilename\x00\x03""abc"_av}),
        TEST_BUF(Msg::ToValidator{"\x01\x00\x00\x00\x0b\x00\x00\x00\x02"_av}),
        TEST_BUF(Msg::ToValidator{requested.file_contents.first(7)}),
        TEST_BUF(Msg::ToMod{32, first_last_flags,
            "\x08\x00\x01\x00\x18\x00\x00\x00\x02\x00\x00\x00\x00\x00\x00\x00"
            "\x02\x00\x00\x00\x07\x00\x00\x00\x00\x00\x00\x00\x07\x00\x00\x00"_av}),
        // without file_contents
        TEST_BUF(Msg::ToFront{19, first_flags, temp_av.first(12)})
    );

    // requested.prepare_range_max_len(requested.file_contents.size() / 2);
    // send_filecontents_request(TEST_BUF(Msg::ToFront{
    //     36, first_last_flags, requested.filecontens_request_av}));

    msg_comparator.run(
        TEST_PROCESS {
            using namespace Cliprdr;
            Buffer buf;
            temp_av = buf.build_ok(RDPECLIP::CB_FILECONTENTS_RESPONSE, [&](OutStream& out){
                out.out_uint32_le(2);
                out.out_copy_bytes(requested.file_contents.from_offset(7));
            });
            channel_ctx->process_server_message(temp_av);
        },
        TEST_BUF(Msg::ToValidator{"\x01\x00\x00\x00\x0a\x00\x00\x00\x02"_av}),
        TEST_BUF(Msg::ToValidator{requested.file_contents.from_offset(7)}),
        TEST_BUF(Msg::Log6{
            "CB_COPYING_PASTING_FILE_FROM_REMOTE_SESSION"
            " file_name=abc size=13 sha256="
            "67f40662fd7aae2942e02a35a519fa2cf628498df498ab3b9c3a74e69f572e4e"_av}),
        TEST_BUF(Msg::ToValidator{"\x03\x00\x00\x00\x04\x00\x00\x00\x02"_av})
    );

    msg_comparator.run(
        TEST_PROCESS { RED_TEST(channel_ctx->dlp_message_accept(FileValidatorId(2))); },
        TEST_BUF(Msg::Log6{"FILE_VERIFICATION direction=DOWN file_name=abc status=ok"_av}),
        TEST_BUF(Msg::ToFront{19, last_flags, requested.file_contents.first(7)})
    );

    if (fdx_ctx && d.always_file_storage) {
        RED_CHECK_FILE_CONTENTS(add_file(*fdx_ctx, ",000002.tfl"), requested.file_contents);
    }

    msg_comparator.run(
        TEST_PROCESS {
            requested.client_file_request(7, 6, StreamId(2));
        },
        TEST_BUF(Msg::ToFront{18, first_last_flags,
             "\x09\x00\x01\x00\x0a\x00\x00\x00\x02\x00\x00\x00""cdefgf"_av})
    );
    //@}


    msg_comparator.run(TEST_PROCESS {
        channel_ctx.reset();
    });

    if (fdx_ctx) {
        (void)fdx_ctx->hash_path.add_file(fdx_basename);
        auto fdx_path = fdx_ctx->record_path.add_file(fdx_basename);

        OutCryptoTransport::HashArray qhash;
        OutCryptoTransport::HashArray fhash;
        fdx_ctx->fdx.close(qhash, fhash);

        RED_CHECK_WORKSPACE(fdx_ctx->wd);

        if (d.always_file_storage) {
            RED_CHECK_FILE_CONTENTS(fdx_path, ut::ascii(
                "\x76\x33\n\x04\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00"
                "\x00\x00\x00\x00\x00\x00\x34\x03\x00\x26\x00"
                "abcmy_session_id/my_session_id,000001.tfl\xd1\xb9\xc9\xdb"
                "\x45\x5c\x70\xb7\xc6\xa7\x02\x25\xa0\x0f\x85\x99\x31\xe4"
                "\x98\xf7\xf5\xe0\x7f\x2c\x96\x2e\x10\x78\xc0\x35\x9f\x5e"
                "\x04\x00\x02\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
                "\x00\x00\x00\x00\x34\x03\x00\x26\x00"
                "abcmy_session_id/my_session_id,000002.tflg\xf4\x06\x62\xfd"
                "\x7a\xae\x29\x42\xe0\x2a\x35\xa5\x19\xfa\x2c\xf6\x28\x49"
                "\x8d\xf4\x98\xab\x3b\x9c\x3a\x74\xe6\x9f\x57\x2e\x4e"_av, 5));
        }
        else {
            RED_CHECK_FILE_CONTENTS(fdx_path, "v3\n"_av);
        }
    }
}