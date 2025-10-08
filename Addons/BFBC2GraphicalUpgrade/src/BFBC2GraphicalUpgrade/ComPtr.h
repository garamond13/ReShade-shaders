#pragma once

#include <cstddef>

template<typename T>
class Com_ptr
{
public:

	Com_ptr() noexcept
		: ptr(nullptr)
	{}

	Com_ptr(T* const other) noexcept
		: ptr(other)
	{
		if (ptr) {
			ptr->AddRef();
		}
	}

	Com_ptr(const Com_ptr<T>& other) noexcept
		: ptr(other.ptr)
	{
		if (ptr) {
			ptr->AddRef();
		}
	}

	Com_ptr(Com_ptr<T>&& other) noexcept
		: ptr(other.ptr)
	{
		other.ptr = nullptr;
	}

	~Com_ptr() noexcept
	{
		if (ptr) {
			ptr->Release();
		}
	}

public:

	Com_ptr<T>& operator=(T* const other) noexcept
	{
		if (ptr != other) {
			if (other) {
				other->AddRef();
			}
			if (ptr) {
				ptr->Release();
			}
			ptr = other;
		}
		return *this;
	}

	Com_ptr<T>& operator=(const Com_ptr<T>& other) noexcept
	{
		if (ptr != other.ptr) {
			if (other.ptr) {
				other.ptr->AddRef();
			}
			if (ptr) {
				ptr->Release();
			}
			ptr = other.ptr;
		}
		return *this;
	}

	Com_ptr<T>& operator=(Com_ptr<T>&& other) noexcept
	{
		if (ptr != other.ptr) {
			if (ptr) {
				ptr->Release();
			}
			ptr = other.ptr;
			other.ptr = nullptr;
		}
		return *this;
	}

public:

	T* operator->() const noexcept
	{
		return ptr;
	}

	T* const* operator&() const noexcept
	{
		return &ptr;
	}

	T** operator&() noexcept
	{
		return &ptr;
	}

	T& operator*() const noexcept
	{
		return *ptr;
	}

	explicit operator bool() const noexcept
	{
		return ptr != nullptr;
	}

public:

	T* get() const noexcept
	{
		return ptr;
	}

	T* const* get_address() const noexcept
	{
		return &ptr;
	}

	T** get_address() noexcept
	{
		return &ptr;
	}

	T** reset_and_get_address() noexcept
	{
		if (ptr) {
			ptr->Release();
			ptr = nullptr;
		}
		return &ptr;
	}

	void attach(T* const other) noexcept
	{
		if (ptr) {
			ptr->Release();
		}
		ptr = other;
	}

	T* detach() noexcept
	{
		T* tmp = ptr;
		ptr = nullptr;
		return tmp;
	}

	template<typename U>
	auto as(Com_ptr<U>& other) const noexcept
	{
		return ptr->QueryInterface(__uuidof(U), reinterpret_cast<void**>(other.reset_and_get_address()));
	}

	template<typename U>
	auto as(U** other) const noexcept
	{
		return ptr->QueryInterface(__uuidof(U), reinterpret_cast<void**>(other));
	}

	void reset() noexcept
	{
		if (ptr) {
			ptr->Release();
			ptr = nullptr;
		}
	}

private:

	T* ptr;
};

template<typename T, typename U>
bool operator==(const Com_ptr<T>& left, const Com_ptr<U>& right) noexcept
{
	return left.get() == right.get();
}

template<typename T, typename U>
bool operator==(const Com_ptr<T>& left, const U* const right) noexcept
{
	return left.get() == right;
}

template<typename T, typename U>
bool operator==(const T* const left, const Com_ptr<U>& right) noexcept
{
	return left == right.get();
}

template<typename T>
bool operator==(const Com_ptr<T>& left, std::nullptr_t) noexcept
{
	return left.get() == nullptr;
}

template<typename T>
bool operator==(std::nullptr_t, const Com_ptr<T>& right) noexcept
{
	return nullptr == right.get();
}

template<typename T, typename U>
bool operator!=(const Com_ptr<T>& left, const Com_ptr<U>& right) noexcept
{
	return left.get() != right.get();
}

template<typename T, typename U>
bool operator!=(const Com_ptr<T>& left, const U* const right) noexcept
{
	return left.get() != right;
}

template<typename T, typename U>
bool operator!=(const T* const left, const Com_ptr<U>& right) noexcept
{
	return left != right.get();
}

template<typename T>
bool operator!=(const Com_ptr<T>& left, std::nullptr_t) noexcept
{
	return left.get() != nullptr;
}

template<typename T, typename U>
bool operator<(const Com_ptr<T>& left, const Com_ptr<U>& right) noexcept
{
	return left.get() < right.get();
}