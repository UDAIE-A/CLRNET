using System;
using System.Buffers;
using System.Collections.Generic;
using System.Text;
using Windows.Data.Json;

namespace System.Text.Json
{
    public enum JsonTokenType
    {
        None,
        StartObject,
        EndObject,
        StartArray,
        EndArray,
        PropertyName,
        String,
        Number,
        True,
        False,
        Null
    }

    public sealed class Utf8JsonWriter : IDisposable
    {
        private readonly IBufferWriter<byte> _bufferWriter;
        private readonly Stack<bool> _containerHasWrittenItem = new Stack<bool>();
        private readonly StringBuilder _builder = new StringBuilder();
        private bool _propertyPending;

        public Utf8JsonWriter(IBufferWriter<byte> bufferWriter)
        {
            _bufferWriter = bufferWriter ?? throw new ArgumentNullException("bufferWriter");
        }

        public void WriteStartObject()
        {
            EnsureValuePosition();
            _builder.Append('{');
            _containerHasWrittenItem.Push(false);
        }

        public void WriteEndObject()
        {
            _builder.Append('}');
            if (_containerHasWrittenItem.Count > 0)
            {
                _containerHasWrittenItem.Pop();
            }

            MarkParentValueWritten();
        }

        public void WriteStartArray()
        {
            EnsureValuePosition();
            _builder.Append('[');
            _containerHasWrittenItem.Push(false);
        }

        public void WriteEndArray()
        {
            _builder.Append(']');
            if (_containerHasWrittenItem.Count > 0)
            {
                _containerHasWrittenItem.Pop();
            }

            MarkParentValueWritten();
        }

        public void WritePropertyName(string propertyName)
        {
            if (propertyName == null)
            {
                throw new ArgumentNullException("propertyName");
            }

            PrepareProperty();
            AppendEscapedString(propertyName);
            _builder.Append(':');
            _propertyPending = true;
        }

        public void WriteStringValue(string value)
        {
            EnsureValuePosition();
            AppendEscapedString(value);
            MarkParentValueWritten();
        }

        public void WriteBooleanValue(bool value)
        {
            EnsureValuePosition();
            _builder.Append(value ? "true" : "false");
            MarkParentValueWritten();
        }

        public void WriteNullValue()
        {
            EnsureValuePosition();
            _builder.Append("null");
            MarkParentValueWritten();
        }

        public void WriteNumberValue(double value)
        {
            EnsureValuePosition();
            _builder.Append(value.ToString(System.Globalization.CultureInfo.InvariantCulture));
            MarkParentValueWritten();
        }

        public void Flush()
        {
            if (_builder.Length == 0)
            {
                return;
            }

            string payload = _builder.ToString();
            int maxBytes = Encoding.UTF8.GetByteCount(payload);
            Span<byte> span = _bufferWriter.GetSpan(maxBytes);
            int written = Encoding.UTF8.GetBytes(payload, span);
            _bufferWriter.Advance(written);
            _builder.Clear();
        }

        public void WriteRaw(ReadOnlySpan<byte> payload)
        {
            if (payload.Length == 0)
            {
                return;
            }

            EnsureValuePosition();
            Span<byte> span = _bufferWriter.GetSpan(payload.Length);
            payload.CopyTo(span);
            _bufferWriter.Advance(payload.Length);
            MarkParentValueWritten();
        }

        private void AppendEscapedString(string value)
        {
            _builder.Append('"');
            if (!string.IsNullOrEmpty(value))
            {
                foreach (char c in value)
                {
                    switch (c)
                    {
                        case '\\':
                            _builder.Append("\\\\");
                            break;
                        case '"':
                            _builder.Append("\\\"");
                            break;
                        case '\n':
                            _builder.Append("\\n");
                            break;
                        case '\r':
                            _builder.Append("\\r");
                            break;
                        case '\t':
                            _builder.Append("\\t");
                            break;
                        default:
                            _builder.Append(c);
                            break;
                    }
                }
            }

            _builder.Append('"');
        }

        private void EnsureValuePosition()
        {
            if (_containerHasWrittenItem.Count > 0)
            {
                if (_propertyPending)
                {
                    _propertyPending = false;
                    return;
                }

                bool hasWritten = _containerHasWrittenItem.Peek();
                if (hasWritten)
                {
                    _builder.Append(',');
                }
            }
        }

        private void PrepareProperty()
        {
            if (_containerHasWrittenItem.Count > 0)
            {
                bool hasWritten = _containerHasWrittenItem.Peek();
                if (hasWritten)
                {
                    _builder.Append(',');
                }
            }
        }

        private void MarkParentValueWritten()
        {
            if (_containerHasWrittenItem.Count > 0)
            {
                _containerHasWrittenItem.Pop();
                _containerHasWrittenItem.Push(true);
            }

            _propertyPending = false;
        }

        public void Dispose()
        {
            Flush();
        }
    }

    public struct Utf8JsonReader
    {
        private readonly JsonValue _root;
        private readonly List<JsonToken> _tokens;
        private int _position;

        public Utf8JsonReader(ReadOnlySpan<byte> utf8Json)
        {
            string text = Encoding.UTF8.GetString(utf8Json.ToArray());
            if (string.IsNullOrEmpty(text))
            {
                _root = JsonNull.CreateNullValue();
            }
            else
            {
                try
                {
                    _root = JsonValue.Parse(text);
                }
                catch (Exception)
                {
                    _root = JsonNull.CreateNullValue();
                }
            }
            _tokens = new List<JsonToken>();
            _position = -1;
            EnqueueTokens(_root, _tokens);
        }

        public JsonTokenType TokenType
        {
            get
            {
                if (_position < 0 || _position >= _tokens.Count)
                {
                    return JsonTokenType.None;
                }

                return _tokens[_position].Type;
            }
        }

        public ReadOnlySpan<byte> ValueSpan
        {
            get
            {
                if (_position < 0 || _position >= _tokens.Count)
                {
                    return default(ReadOnlySpan<byte>);
                }

                string value = _tokens[_position].StringValue;
                return value == null ? default(ReadOnlySpan<byte>) : Encoding.UTF8.GetBytes(value);
            }
        }

        public bool GetBoolean()
        {
            return _tokens[_position].BooleanValue;
        }

        public double GetDouble()
        {
            return _tokens[_position].NumberValue;
        }

        public bool Read()
        {
            if (_position + 1 >= _tokens.Count)
            {
                return false;
            }

            _position++;
            return true;
        }

        private static void EnqueueTokens(JsonValue value, List<JsonToken> tokens)
        {
            switch (value.ValueType)
            {
                case JsonValueType.Null:
                    tokens.Add(JsonToken.Null());
                    break;
                case JsonValueType.Boolean:
                    tokens.Add(JsonToken.Boolean(value.GetBoolean()));
                    break;
                case JsonValueType.Number:
                    tokens.Add(JsonToken.Number(value.GetNumber()));
                    break;
                case JsonValueType.String:
                    tokens.Add(JsonToken.String(value.GetString()));
                    break;
                case JsonValueType.Object:
                    tokens.Add(JsonToken.StartObject());
                    foreach (KeyValuePair<string, JsonValue> property in value.GetObject())
                    {
                        tokens.Add(JsonToken.Property(property.Key));
                        EnqueueTokens(property.Value, tokens);
                    }

                    tokens.Add(JsonToken.EndObject());
                    break;
                case JsonValueType.Array:
                    tokens.Add(JsonToken.StartArray());
                    foreach (JsonValue item in value.GetArray())
                    {
                        EnqueueTokens(item, tokens);
                    }

                    tokens.Add(JsonToken.EndArray());
                    break;
            }
        }

        private struct JsonToken
        {
            public JsonTokenType Type;
            public string StringValue;
            public double NumberValue;
            public bool BooleanValue;

            public static JsonToken StartObject()
            {
                return new JsonToken { Type = JsonTokenType.StartObject };
            }

            public static JsonToken EndObject()
            {
                return new JsonToken { Type = JsonTokenType.EndObject };
            }

            public static JsonToken StartArray()
            {
                return new JsonToken { Type = JsonTokenType.StartArray };
            }

            public static JsonToken EndArray()
            {
                return new JsonToken { Type = JsonTokenType.EndArray };
            }

            public static JsonToken Property(string name)
            {
                return new JsonToken { Type = JsonTokenType.PropertyName, StringValue = name };
            }

            public static JsonToken String(string value)
            {
                return new JsonToken { Type = JsonTokenType.String, StringValue = value };
            }

            public static JsonToken Number(double value)
            {
                return new JsonToken { Type = JsonTokenType.Number, NumberValue = value };
            }

            public static JsonToken Boolean(bool value)
            {
                return new JsonToken { Type = value ? JsonTokenType.True : JsonTokenType.False, BooleanValue = value };
            }

            public static JsonToken Null()
            {
                return new JsonToken { Type = JsonTokenType.Null };
            }
        }
    }
}
