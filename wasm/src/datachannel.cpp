/**
 * Copyright (c) 2017-2022 Paul-Louis Ageneau
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "datachannel.hpp"

#include <emscripten/emscripten.h>

#include <chrono>
#include <exception>
#include <stdexcept>

extern "C" {
extern void js_rtcDeleteDataChannel(int dc);
extern int js_rtcGetDataChannelLabel(int dc, char *buffer, int size);
extern int js_rtcGetDataChannelUnordered(int dc);
extern int js_rtcGetDataChannelMaxPacketLifeTime(int dc);
extern int js_rtcGetDataChannelMaxRetransmits(int dc);
extern void js_rtcSetOpenCallback(int dc, void (*openCallback)(void *));
extern void js_rtcSetErrorCallback(int dc, void (*errorCallback)(const char *, void *));
extern void js_rtcSetMessageCallback(int dc, void (*messageCallback)(const char *, int, void *));
extern void js_rtcSetBufferedAmountLowCallback(int dc, void (*bufferedAmountLowCallback)(void *));
extern int js_rtcGetBufferedAmount(int dc);
extern void js_rtcSetBufferedAmountLowThreshold(int dc, int threshold);
extern int js_rtcSendMessage(int dc, const char *buffer, int size);
extern void js_rtcSetUserPointer(int i, void *ptr);
}

namespace rtc {

using std::function;

void DataChannel::OpenCallback(void *ptr) {
	DataChannel *d = static_cast<DataChannel *>(ptr);
	if (d)
		d->triggerOpen();
}

void DataChannel::ErrorCallback(const char *error, void *ptr) {
	DataChannel *d = static_cast<DataChannel *>(ptr);
	if (d)
		d->triggerError(string(error ? error : "unknown"));
}

void DataChannel::MessageCallback(const char *data, int size, void *ptr) {
	DataChannel *d = static_cast<DataChannel *>(ptr);
	if (d) {
		if (data) {
			if (size >= 0) {
				auto *b = reinterpret_cast<const byte *>(data);
				d->triggerMessage(binary(b, b + size));
			} else {
				d->triggerMessage(string(data));
			}
		} else {
			d->close();
			d->triggerClosed();
		}
	}
}

void DataChannel::BufferedAmountLowCallback(void *ptr) {
	DataChannel *d = static_cast<DataChannel *>(ptr);
	if (d) {
		d->triggerBufferedAmountLow();
	}
}

DataChannel::DataChannel(int id) : mId(id), mConnected(false) {
	js_rtcSetUserPointer(mId, this);
	js_rtcSetOpenCallback(mId, OpenCallback);
	js_rtcSetErrorCallback(mId, ErrorCallback);
	js_rtcSetMessageCallback(mId, MessageCallback);
	js_rtcSetBufferedAmountLowCallback(mId, BufferedAmountLowCallback);

	char str[256];
	js_rtcGetDataChannelLabel(mId, str, 256);
	mLabel = str;
}

DataChannel::~DataChannel() { close(); }

void DataChannel::close() {
	mConnected = false;
	if (mId) {
		js_rtcDeleteDataChannel(mId);
		mId = 0;
	}
}

bool DataChannel::send(message_variant message) {
	if (!mId)
		return false;

	return std::visit(
	    overloaded{[this](const binary &b) {
		               auto data = reinterpret_cast<const char *>(b.data());
		               return js_rtcSendMessage(mId, data, int(b.size())) >= 0;
	               },
	               [this](const string &s) { return js_rtcSendMessage(mId, s.c_str(), -1) >= 0; }},
	    std::move(message));
}

bool DataChannel::send(const byte *data, size_t size) {
	if (!mId)
		return false;

	return js_rtcSendMessage(mId, reinterpret_cast<const char *>(data), int(size)) >= 0;
}

bool DataChannel::isOpen() const { return mConnected; }

bool DataChannel::isClosed() const { return mId == 0; }

size_t DataChannel::bufferedAmount() const {
	if (!mId)
		return 0;

	int ret = js_rtcGetBufferedAmount(mId);
	if (ret < 0)
		return 0;

	return size_t(ret);
}

std::string DataChannel::label() const { return mLabel; }

Reliability DataChannel::reliability() const {
	Reliability reliability = {};

	if (!mId)
		return reliability;

	reliability.unordered = js_rtcGetDataChannelUnordered(mId) ? true : false;

	int maxRetransmits = js_rtcGetDataChannelMaxRetransmits(mId);
	int maxPacketLifeTime = js_rtcGetDataChannelMaxPacketLifeTime(mId);

	if (maxRetransmits >= 0)
		reliability.maxRetransmits = unsigned(maxRetransmits);

	if (maxPacketLifeTime >= 0)
		reliability.maxPacketLifeTime = std::chrono::milliseconds(maxPacketLifeTime);

	return reliability;
}

void DataChannel::setBufferedAmountLowThreshold(size_t amount) {
	if (!mId)
		return;

	js_rtcSetBufferedAmountLowThreshold(mId, int(amount));
}

void DataChannel::triggerOpen() {
	mConnected = true;
	Channel::triggerOpen();
}

} // namespace rtc
