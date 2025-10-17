using System;
using System.Buffers;
using System.IO;
using System.Runtime.Serialization.Json;
using System.Text;

namespace System.Text.Json
{
    public sealed class JsonSerializerOptions
    {
        public bool WriteIndented { get; set; }
    }

    public static class JsonSerializer
    {
        public static string Serialize<TValue>(TValue value, JsonSerializerOptions options = null)
        {
            using (MemoryStream stream = new MemoryStream())
            {
                Serialize(stream, value, options);
                return Encoding.UTF8.GetString(stream.ToArray(), 0, (int)stream.Length);
            }
        }

        public static void Serialize<TValue>(Utf8JsonWriter writer, TValue value, JsonSerializerOptions options = null)
        {
            if (writer == null)
            {
                throw new ArgumentNullException("writer");
            }

            using (MemoryStream stream = new MemoryStream())
            {
                Serialize(stream, value, options);
                writer.WriteRaw(new ReadOnlySpan<byte>(stream.ToArray()));
                writer.Flush();
            }
        }

        private static void Serialize<TValue>(Stream stream, TValue value, JsonSerializerOptions options)
        {
            DataContractJsonSerializerSettings settings = new DataContractJsonSerializerSettings
            {
                UseSimpleDictionaryFormat = true
            };

            DataContractJsonSerializer serializer = new DataContractJsonSerializer(typeof(TValue), settings);
            serializer.WriteObject(stream, value);
        }

        public static TValue Deserialize<TValue>(string json, JsonSerializerOptions options = null)
        {
            if (json == null)
            {
                throw new ArgumentNullException("json");
            }

            byte[] data = Encoding.UTF8.GetBytes(json);
            return Deserialize<TValue>(new ReadOnlySpan<byte>(data), options);
        }

        public static TValue Deserialize<TValue>(ReadOnlySpan<byte> utf8Json, JsonSerializerOptions options = null)
        {
            using (MemoryStream stream = new MemoryStream(utf8Json.ToArray()))
            {
                DataContractJsonSerializerSettings settings = new DataContractJsonSerializerSettings
                {
                    UseSimpleDictionaryFormat = true
                };

                DataContractJsonSerializer serializer = new DataContractJsonSerializer(typeof(TValue), settings);
                return (TValue)serializer.ReadObject(stream);
            }
        }
    }

}
