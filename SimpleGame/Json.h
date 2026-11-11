#pragma once

#include <map>
#include <string>
#include <vector>

// 아주 작은 JSON 읽기 도구(쓰기는 Monitor.cpp의 JsonWriter). glTF(.glb)의 JSON 덩어리와 Resource 폴더의 설명
// 파일(*.json)을 읽는 데 쓴다. 값 하나가 null/불/수/문자열/배열/객체 중 하나를 들고, 없는 키·범위 밖 번호를
// 물으면 null 값을 돌려줘서 읽는 쪽이 하나하나 검사하지 않아도 되게 한다(기본값을 넘겨 받는다).
class JsonValue
{
public:
	enum class Type
	{
		Null,
		Bool,
		Number,
		String,
		Array,
		Object,
	};

	Type GetType() const { return m_Type; }
	bool IsNull() const { return m_Type == Type::Null; }
	bool IsArray() const { return m_Type == Type::Array; }
	bool IsObject() const { return m_Type == Type::Object; }
	bool IsNumber() const { return m_Type == Type::Number; }
	bool IsString() const { return m_Type == Type::String; }

	bool AsBool(bool fallback = false) const { return (m_Type == Type::Bool) ? m_Bool : fallback; }
	double AsNumber(double fallback = 0.0) const { return (m_Type == Type::Number) ? m_Number : fallback; }
	float AsFloat(float fallback = 0.f) const { return (m_Type == Type::Number) ? (float)m_Number : fallback; }
	int AsInt(int fallback = 0) const { return (m_Type == Type::Number) ? (int)m_Number : fallback; }
	const std::string& AsString() const;

	// 배열의 길이와 원소(배열이 아니거나 범위 밖이면 null).
	size_t Size() const { return m_Items.size(); }
	const JsonValue& At(size_t index) const;

	// 객체의 키(없으면 null).
	const JsonValue& operator[](const char* key) const;
	bool Has(const char* key) const;
	const std::map<std::string, JsonValue>& Members() const { return m_Members; }

	// text 전체를 읽는다. 틀린 곳이 있으면 false와 함께 outError에 위치·이유를 적는다.
	static bool Parse(const char* text, size_t length, JsonValue& outValue, std::string& outError);

private:
	friend class JsonParser;

	Type m_Type = Type::Null;
	bool m_Bool = false;
	double m_Number = 0.0;
	std::string m_String;
	std::vector<JsonValue> m_Items;
	std::map<std::string, JsonValue> m_Members;
};
