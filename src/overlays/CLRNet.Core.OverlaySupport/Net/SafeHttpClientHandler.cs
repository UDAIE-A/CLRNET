using System;
using System.Collections.Generic;
using System.Net.Http;
using System.Threading;
using System.Threading.Tasks;
using System.Runtime.InteropServices.WindowsRuntime;
using Windows.Web.Http;
using Windows.Web.Http.Filters;
using Windows.Web.Http.Headers;
using Windows.Storage.Streams;

namespace System.Net.Http
{
    /// <summary>
    /// HttpMessageHandler that wraps the WinRT HTTP client to provide predictable cookie and redirect behaviour.
    /// </summary>
    public sealed class SafeHttpClientHandler : HttpMessageHandler, IDisposable
    {
        private readonly HttpBaseProtocolFilter _filter;
        private readonly Windows.Web.Http.HttpClient _client;
        private readonly CookieManager _cookieManager;
        private readonly object _sync = new object();

        public SafeHttpClientHandler()
        {
            _filter = new HttpBaseProtocolFilter
            {
                AllowAutoRedirect = false,
                AutomaticDecompression = true,
                CacheControl = { ReadBehavior = HttpCacheReadBehavior.MostRecent, WriteBehavior = HttpCacheWriteBehavior.Default }
            };

            _filter.IgnorableServerCertificateErrors.Add(Windows.Security.Cryptography.Certificates.ChainValidationResult.InvalidName);
            _filter.IgnorableServerCertificateErrors.Add(Windows.Security.Cryptography.Certificates.ChainValidationResult.RevocationFailure);

            _cookieManager = _filter.CookieManager;
            _client = new Windows.Web.Http.HttpClient(_filter);
        }

        public IList<HttpCookie> Cookies
        {
            get { return new List<HttpCookie>(_cookieManager.GetCookies(new Uri("http://localhost"))); }
        }

        protected override async Task<HttpResponseMessage> SendAsync(HttpRequestMessage request, CancellationToken cancellationToken)
        {
            if (request == null)
            {
                throw new ArgumentNullException("request");
            }

            Windows.Web.Http.HttpRequestMessage winRtMessage = await CreateWinRtRequestAsync(request).ConfigureAwait(false);
            Windows.Web.Http.HttpResponseMessage response = await _client.SendRequestAsync(winRtMessage).AsTask(cancellationToken).ConfigureAwait(false);
            HttpResponseMessage managedResponse = await ConvertResponseAsync(response).ConfigureAwait(false);

            if (IsRedirect(managedResponse.StatusCode) && request.Headers.AllowRedirect())
            {
                Uri redirectUri = managedResponse.Headers.Location;
                if (redirectUri != null)
                {
                    HttpRequestMessage followUp = new HttpRequestMessage(HttpMethod.Get, redirectUri);
                    foreach (KeyValuePair<string, IEnumerable<string>> header in request.Headers)
                    {
                        followUp.Headers.TryAddWithoutValidation(header.Key, header.Value);
                    }

                    return await SendAsync(followUp, cancellationToken).ConfigureAwait(false);
                }
            }

            return managedResponse;
        }

        private static bool IsRedirect(System.Net.HttpStatusCode statusCode)
        {
            int code = (int)statusCode;
            return code >= 300 && code <= 399;
        }

        private static async Task<Windows.Web.Http.HttpRequestMessage> CreateWinRtRequestAsync(HttpRequestMessage request)
        {
            Windows.Web.Http.HttpRequestMessage winRtRequest = new Windows.Web.Http.HttpRequestMessage(new Windows.Web.Http.HttpMethod(request.Method.Method), request.RequestUri);
            foreach (KeyValuePair<string, IEnumerable<string>> header in request.Headers)
            {
                winRtRequest.Headers.TryAppendWithoutValidation(header.Key, string.Join(",", header.Value));
            }

            if (request.Content != null)
            {
                byte[] payload = await request.Content.ReadAsByteArrayAsync().ConfigureAwait(false);
                winRtRequest.Content = new HttpBufferContent(payload.AsBuffer());
                foreach (KeyValuePair<string, IEnumerable<string>> header in request.Content.Headers)
                {
                    winRtRequest.Content.Headers.TryAppendWithoutValidation(header.Key, string.Join(",", header.Value));
                }
            }

            return winRtRequest;
        }

        private static async Task<HttpResponseMessage> ConvertResponseAsync(Windows.Web.Http.HttpResponseMessage response)
        {
            HttpResponseMessage managed = new HttpResponseMessage((System.Net.HttpStatusCode)response.StatusCode)
            {
                ReasonPhrase = response.ReasonPhrase,
                RequestMessage = null,
                Version = new Version(1, 1)
            };

            foreach (KeyValuePair<string, string> header in response.Headers)
            {
                managed.Headers.TryAddWithoutValidation(header.Key, header.Value);
            }

            if (response.Content != null)
            {
                IBuffer buffer = await response.Content.ReadAsBufferAsync().AsTask().ConfigureAwait(false);
                byte[] data = buffer == null ? new byte[0] : buffer.ToArray();
                managed.Content = new ByteArrayContent(data);
                foreach (KeyValuePair<string, string> header in response.Content.Headers)
                {
                    managed.Content.Headers.TryAddWithoutValidation(header.Key, header.Value);
                }
            }

            return managed;
        }

        public void Dispose()
        {
            lock (_sync)
            {
                _client.Dispose();
                _filter.Dispose();
            }
        }
    }

    internal static class HttpHeadersExtensions
    {
        public static bool AllowRedirect(this HttpRequestHeaders headers)
        {
            if (headers == null)
            {
                return true;
            }

            IEnumerable<string> values;
            if (headers.TryGetValues("X-CLRNET-AllowRedirect", out values))
            {
                foreach (string value in values)
                {
                    if (string.Equals(value, "false", StringComparison.OrdinalIgnoreCase))
                    {
                        return false;
                    }
                }
            }

            return true;
        }

        public static Windows.Web.Http.Headers.HttpContentHeaderCollection Headers(this Windows.Web.Http.IHttpContent content)
        {
            return content.Headers;
        }
    }
}
