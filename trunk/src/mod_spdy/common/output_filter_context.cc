// Copyright 2010 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "mod_spdy/common/output_filter_context.h"

#include "base/scoped_ptr.h"
#include "mod_spdy/common/connection_context.h"
#include "mod_spdy/common/header_populator_interface.h"
#include "mod_spdy/common/output_stream_interface.h"
#include "net/spdy/spdy_framer.h"

namespace mod_spdy {

namespace {

class SpdyFrameSender {
 public:
  SpdyFrameSender(spdy::SpdyStreamId stream_id,
                  spdy::SpdyFramer* framer,
                  OutputStreamInterface* output_stream)
      : stream_id_(stream_id),
        framer_(framer),
        output_stream_(output_stream) {}
  ~SpdyFrameSender() {}

  bool SendFrame(const spdy::SpdyFrame* frame) {
    return output_stream_->Write(frame->data(),
                                 spdy::SpdyFrame::size() + frame->length());
  }

  bool SendNopFrame() {
    scoped_ptr<spdy::SpdyFrame> frame(spdy::SpdyFramer::CreateNopFrame());
    return SendFrame(frame.get());
  }

  bool SendSynReplyFrame(spdy::SpdyHeaderBlock* headers, bool fin_stream) {
    const spdy::SpdyControlFlags flags =
        (fin_stream ? spdy::CONTROL_FLAG_FIN : spdy::CONTROL_FLAG_NONE);
    scoped_ptr<spdy::SpdyFrame> frame(
        framer_->CreateSynReply(stream_id_, flags,
                                true,  // use compression
                                headers));
    return SendFrame(frame.get());
  }

  bool SendDataFrame(const char* data, size_t num_bytes, bool fin_stream) {
    const spdy::SpdyDataFlags flags =
        (fin_stream ? spdy::DATA_FLAG_FIN : spdy::DATA_FLAG_NONE);
    scoped_ptr<spdy::SpdyFrame> frame(
        framer_->CreateDataFrame(stream_id_, data, num_bytes, flags));
    return SendFrame(frame.get());
  }

 private:
  const spdy::SpdyStreamId stream_id_;
  spdy::SpdyFramer* framer_;
  OutputStreamInterface* output_stream_;

  DISALLOW_COPY_AND_ASSIGN(SpdyFrameSender);
};

}  // namespace

OutputFilterContext::OutputFilterContext(ConnectionContext* conn_context)
    : conn_context_(conn_context),
      headers_have_been_sent_(false) {}

OutputFilterContext::~OutputFilterContext() {}

bool OutputFilterContext::SendHeaders(
    spdy::SpdyStreamId stream_id,
    const HeaderPopulatorInterface& populator,
    bool is_end_of_stream,
    OutputStreamInterface* output_stream) {
  headers_have_been_sent_ = true;
  SpdyFrameSender sender(stream_id, conn_context_->output_framer(),
                         output_stream);
  spdy::SpdyHeaderBlock headers;
  populator.Populate(&headers);
  return sender.SendSynReplyFrame(&headers, is_end_of_stream);
}

bool OutputFilterContext::SendData(spdy::SpdyStreamId stream_id,
                                   const char* input_data,
                                   size_t input_size,
                                   bool is_end_of_stream,
                                   OutputStreamInterface* output_stream) {
  SpdyFrameSender sender(stream_id, conn_context_->output_framer(),
                         output_stream);
  return sender.SendDataFrame(input_data, input_size, is_end_of_stream);
}

}  // namespace mod_spdy
