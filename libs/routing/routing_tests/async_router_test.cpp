#include "testing/testing.hpp"

#include "routing/async_router.hpp"
#include "routing/route.hpp"
#include "routing/router.hpp"
#include "routing/routing_callbacks.hpp"

#include "routing/routing_tests/tools.hpp"

#include "base/scope_guard.hpp"

#include <chrono>
#include <condition_variable>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace async_router_test
{
using namespace routing;
using namespace std::placeholders;
using namespace std;

class DummyRouter : public IRouter
{
  RouterResultCode m_result;

public:
  DummyRouter(RouterResultCode code, set<string> const & absent) : m_result(code) {}

  // IRouter overrides:
  string GetName() const override { return "Dummy"; }
  void SetGuides(GuidesTracks && /* guides */) override {}
  RouterResultCode CalculateRoute(Checkpoints const & checkpoints, m2::PointD const & startDirection,
                                  bool adjustToPrevRoute, RouterDelegate const & delegate,
                                  RoutesResult & result) override
  {
    Route route;
    route.SetGeometry(checkpoints.GetPoints().cbegin(), checkpoints.GetPoints().cend());
    result.MakeFrom(GetName(), std::move(route));
    return m_result;
  }

  bool FindClosestProjectionToRoad(m2::PointD const & point, m2::PointD const & direction, double radius,
                                   EdgeProj & proj) override
  {
    return false;
  }
};

struct DummyRoutingCallbacks
{
  vector<RouterResultCode> m_codes;
  vector<set<string>> m_absent;
  condition_variable m_cv;
  mutex m_lock;
  uint32_t const m_expected;
  uint32_t m_called;

  explicit DummyRoutingCallbacks(uint32_t expectedCalls) : m_expected(expectedCalls), m_called(0) {}

  // ReadyCallbackOwnership callback
  void operator()(shared_ptr<Route> route, RouterResultCode code)
  {
    CHECK(route, ());
    m_codes.push_back(code);
    TestAndNotifyReadyCallbacks();
  }

  // NeedMoreMapsCallback callback
  void operator()(uint64_t routeId, set<string> const & absent)
  {
    m_codes.push_back(RouterResultCode::NeedMoreMaps);
    m_absent.emplace_back(absent);
    TestAndNotifyReadyCallbacks();
  }

  void WaitFinish()
  {
    unique_lock<mutex> lk(m_lock);
    return m_cv.wait(lk, [this] { return m_called == m_expected; });
  }

  void TestAndNotifyReadyCallbacks()
  {
    {
      lock_guard<mutex> calledLock(m_lock);
      ++m_called;
      TEST_LESS_OR_EQUAL(m_called, m_expected, ("The result callback called more times than expected."));
    }
    m_cv.notify_all();
  }
};

// TODO(o.khlopkova) Uncomment and update these tests.

// UNIT_CLASS_TEST(AsyncGuiThreadTest, NeedMoreMapsSignalTest)
//{
//  set<string> const absentData({"test1", "test2"});
//  unique_ptr<IOnlineFetcher> fetcher(new DummyFetcher(absentData));
//  unique_ptr<IRouter> router(new DummyRouter(RouterResultCode::NoError, {}));
//  DummyRoutingCallbacks resultCallback(2 /* expectedCalls */);
//  AsyncRouter async(DummyStatisticsCallback, nullptr /* pointCheckCallback */);
//  async.SetRouter(std::move(router), std::move(fetcher));
//  async.CalculateRoute(Checkpoints({1, 2} /* start */, {5, 6} /* finish */), {3, 4}, false,
//                       bind(ref(resultCallback), _1, _2) /* readyCallback */,
//                       bind(ref(resultCallback), _1, _2) /* needMoreMapsCallback */,
//                       nullptr /* removeRouteCallback */, nullptr /* progressCallback */);
//
//  resultCallback.WaitFinish();
//
//  TEST_EQUAL(resultCallback.m_codes.size(), 2, ());
//  TEST_EQUAL(resultCallback.m_codes[0], RouterResultCode::NoError, ());
//  TEST_EQUAL(resultCallback.m_codes[1], RouterResultCode::NeedMoreMaps, ());
//  TEST_EQUAL(resultCallback.m_absent.size(), 2, ());
//  TEST(resultCallback.m_absent[0].empty(), ());
//  TEST_EQUAL(resultCallback.m_absent[1].size(), 2, ());
//  TEST_EQUAL(resultCallback.m_absent[1], absentData, ());
//}

// UNIT_CLASS_TEST(AsyncGuiThreadTest, StandardAsyncFogTest)
//{
//  unique_ptr<IOnlineFetcher> fetcher(new DummyFetcher({}));
//  unique_ptr<IRouter> router(new DummyRouter(RouterResultCode::NoError, {}));
//  DummyRoutingCallbacks resultCallback(1 /* expectedCalls */);
//  AsyncRouter async(DummyStatisticsCallback, nullptr /* pointCheckCallback */);
//  async.SetRouter(std::move(router), std::move(fetcher));
//  async.CalculateRoute(Checkpoints({1, 2} /* start */, {5, 6} /* finish */), {3, 4}, false,
//                       bind(ref(resultCallback), _1, _2), nullptr /* needMoreMapsCallback */,
//                       nullptr /* progressCallback */, nullptr /* removeRouteCallback */);
//
//  resultCallback.WaitFinish();
//
//  TEST_EQUAL(resultCallback.m_codes.size(), 1, ());
//  TEST_EQUAL(resultCallback.m_codes[0], RouterResultCode::NoError, ());
//  TEST_EQUAL(resultCallback.m_absent.size(), 1, ());
//  TEST(resultCallback.m_absent[0].empty(), ());
//}
// Parks inside CalculateRoute until the test releases it and records direct router calls.
class BlockingRouter : public IRouter
{
public:
  string GetName() const override { return "blocking"; }
  void ClearState() override {}
  void SetGuides(GuidesTracks && /* guides */) override {}
  void SwapAltRouteToActive() override { ++m_swaps; }

  RouterResultCode CalculateRoute(Checkpoints const & checkpoints, m2::PointD const & /* startDirection */,
                                  bool /* adjustToPrevRoute */, RouterDelegate const & /* delegate */,
                                  RoutesResult & result) override
  {
    Notify(m_started);
    {
      unique_lock l(m_mutex);
      m_cv.wait(l, [this] { return m_release; });
    }

    Route route;
    route.SetGeometry(checkpoints.GetPoints().cbegin(), checkpoints.GetPoints().cend());
    vector<RouteSegment> segments;
    RouteSegmentsFrom({}, checkpoints.GetPoints(), {}, {}, segments);
    route.SetRouteSegments(std::move(segments));
    auto const & points = checkpoints.GetPoints();
    route.SetSubroutes(vector<Route::SubrouteAttrs>{Route::SubrouteAttrs(
        geometry::PointWithAltitude(points.front(), geometry::kDefaultAltitudeMeters),
        geometry::PointWithAltitude(points.back(), geometry::kDefaultAltitudeMeters), 0, points.size() - 1)});
    result.MakeFrom(GetName(), std::move(route));
    return RouterResultCode::NoError;
  }

  bool FindClosestProjectionToRoad(m2::PointD const & /* point */, m2::PointD const & /* direction */,
                                   double /* radius */, EdgeProj & /* proj */) override
  {
    ++m_projections;
    return false;
  }

  bool WaitStarted(std::chrono::seconds timeout)
  {
    unique_lock l(m_mutex);
    return m_cv.wait_for(l, timeout, [this] { return m_started; });
  }

  void Release() { Notify(m_release); }

  size_t m_swaps = 0;
  size_t m_projections = 0;

private:
  void Notify(bool & flag)
  {
    {
      lock_guard l(m_mutex);
      flag = true;
    }
    m_cv.notify_all();
  }

  mutex m_mutex;
  condition_variable m_cv;
  bool m_started = false;
  bool m_release = false;
};

// IRouter::CalculateRoute runs without m_guard and mutates state also used by the synchronous
// gui-thread entry points, so AsyncRouter must reject those calls until the result is ready.
UNIT_CLASS_TEST(AsyncGuiThreadTest, RouterCallsWhileCalculating)
{
  auto router = make_unique<BlockingRouter>();
  auto * blocking = router.get();

  // The gui thread outlives the test body, so the ready callback must not capture a local by
  // reference: a TEST throwing above would leave it pointing at a destroyed promise.
  auto readyPromise = make_shared<promise<uint64_t>>();
  auto readyFuture = readyPromise->get_future();

  AsyncRouter async(nullptr /* pointCheckCallback */);
  async.SetRouter(std::move(router), nullptr /* absentRegionsFinder */);
  async.CalculateRoute(Checkpoints({1, 2} /* start */, {5, 6} /* finish */), {3, 4} /* direction */,
                       false /* adjustToPrevRoute */, [readyPromise](shared_ptr<RoutesResult> result, RouterResultCode)
  { readyPromise->set_value(result->m_routesId); }, nullptr /* needMoreMapsCallback */,
                       nullptr /* removeRouteCallback */, nullptr /* progressCallback */);

  // ~AsyncRouter joins the routing thread, so the router must be let go even if a TEST throws.
  SCOPE_GUARD(releaseRouter, [blocking] { blocking->Release(); });
  TEST(blocking->WaitStarted(std::chrono::seconds(30)), ("Route calculation did not start."));

  EdgeProj proj;
  m2::PointD const point(1.0, 2.0);
  m2::PointD const direction(0.0, 1.0);

  TEST(!async.SwapAltRouteToActive(1), ("The swap must not touch a calculating router."));
  TEST_EQUAL(blocking->m_swaps, 0, ());
  TEST(!async.FindClosestProjectionToRoad(point, direction, 50.0 /* radius */, proj),
       ("The projection lookup must not touch a calculating router."));
  TEST_EQUAL(blocking->m_projections, 0, ());

  blocking->Release();
  TEST(readyFuture.wait_for(std::chrono::seconds(30)) == future_status::ready, ("Route was not built."));
  auto const routesId = readyFuture.get();

  TEST(!async.SwapAltRouteToActive(routesId + 1), ("A stale result must not swap the router caches."));
  TEST_EQUAL(blocking->m_swaps, 0, ());
  TEST(async.SwapAltRouteToActive(routesId), ("The ready result must swap."));
  TEST_EQUAL(blocking->m_swaps, 1, ());
  // The mock finds nothing, the point is that the call reaches it now.
  async.FindClosestProjectionToRoad(point, direction, 50.0 /* radius */, proj);
  TEST_EQUAL(blocking->m_projections, 1, ("An idle router must answer the projection lookup."));
}
}  //  namespace async_router_test
