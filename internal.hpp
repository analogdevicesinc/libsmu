#pragma once
#include <libusb-1.0/libusb.h>
#include <vector>

// Wrapper for a collection of libusb transfers
struct Transfers {
	std::vector<libusb_transfer*> m_transfers;

	void alloc(unsigned count, libusb_device_handle* handle,
	           unsigned char endpoint, unsigned char type, size_t buf_size,
	           unsigned timeout, libusb_transfer_cb_fn callback, void* user_data) {
		m_transfers.resize(count, NULL);
		for (size_t i=0; i<count; i++) {
			auto t = m_transfers[i] = libusb_alloc_transfer(0);
			t->dev_handle = handle;
			t->flags = LIBUSB_TRANSFER_FREE_BUFFER;
			t->endpoint = endpoint;
			t->type = type;
			t->timeout = timeout;
			t->length = buf_size;
			t->callback = callback;
			t->user_data = user_data;
			t->buffer = (uint8_t*) malloc(buf_size);
		}
	}

	void clear() {
		for (auto i: m_transfers) {
			libusb_free_transfer(i);
		}
		m_transfers.clear();
	}

	size_t size() {
		return m_transfers.size();
	}

	~Transfers() {
		clear();
	}

	typedef std::vector<libusb_transfer*>::iterator iterator;
	typedef std::vector<libusb_transfer*>::const_iterator const_iterator;
	iterator begin() { return m_transfers.begin(); }
	const_iterator begin() const { return m_transfers.begin(); }
	iterator end() { return m_transfers.end(); }
	const_iterator end() const { return m_transfers.end(); }
};
