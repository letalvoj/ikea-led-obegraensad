import { type Component, For, Index, Show } from "solid-js";

import Button from "./components/button";
import { Layout } from "./components/layout/layout";
import { useStore } from "./contexts/store";
import { useToast } from "./contexts/toast";

// Scheduler commands go over the device connection (WebSocket or Bluetooth); the
// device answers with an updated "info" event.
const useScheduleControls = () => {
  const [store, actions] = useStore();
  const { toast } = useToast();

  const send = (message: object) => actions.send(JSON.stringify(message));

  const toggle = () => {
    if (store.isActiveScheduler) {
      send({ event: "schedule-stop" });
      toast("Stopped schedule", 2000);
    } else if (store.schedule.length === 0) {
      toast("Cannot start empty schedule", 2000);
    } else {
      send({ event: "schedule", schedule: store.schedule });
      toast("Schedule started", 2000);
    }
  };

  const reset = () => {
    send({ event: "schedule-clear" });
    toast("Reset schedule", 2000);
  };

  return { toggle, reset };
};

export const ResetScheduleButton = () => {
  const { reset } = useScheduleControls();

  return (
    <button type="button" onClick={reset} class="w-full btn">
      Reset Scheduler
    </button>
  );
};

export const ToggleScheduleButton = () => {
  const [store] = useStore();
  const { toggle } = useScheduleControls();

  return (
    <button type="button" onClick={toggle} class="w-full btn">
      {store.isActiveScheduler ? "Stop Scheduler" : "Start Scheduler"}
    </button>
  );
};

const Scheduler: Component = () => {
  const [store, actions] = useStore();
  const { toggle: handleToggleScheduler } = useScheduleControls();

  const handleAddItem = () => {
    const defaultPluginId = store.plugins.length > 0 ? store.plugins[0].id : 1;
    actions.setSchedule([...store.schedule, { pluginId: defaultPluginId, duration: 1 }]);
  };

  const handleRemoveItem = (index: number) => {
    actions.setSchedule(store.schedule.filter((_, i) => i !== index));
  };

  const handlePluginChange = (index: number, pluginId: number) => {
    actions.setSchedule(
      store.schedule.map((item, i) => (i === index ? { ...item, pluginId } : item)),
    );
  };

  const handleDurationChange = (index: number, duration: number) => {
    actions.setSchedule(
      store.schedule.map((item, i) => (i === index ? { ...item, duration } : item)),
    );
  };

  return (
    <Layout
      content={
        <div class="space-y-3 p-5 pb-24 lg:pb-5">
          <h3 class="page-title">Scheduler</h3>

          <div class="bg-white p-4 lg:p-6">
            <div class="space-y-2">
              <Show
                when={store.schedule?.length > 0}
                fallback={
                  <div class="text-center py-8">
                    <div class="text-md text-muted italic mb-4">No schedule set</div>
                    <button type="button" onClick={handleAddItem} class="btn px-4">
                      <i class="fa-solid fa-plus mr-2" />
                      Add First Plugin
                    </button>
                  </div>
                }
              >
                <Index each={store.schedule}>
                  {(item, index) => (
                    <div
                      class={`flex flex-col lg:flex-row items-stretch lg:items-center gap-3 lg:gap-4 p-3 lg:p-4 rounded-lg shadow-sm border hover:border-line transition-all duration-200 ${
                        store.plugin === item().pluginId
                          ? "bg-accent/30 border-accent"
                          : "bg-white border-line"
                      }`}
                    >
                      <Show
                        when={store.isActiveScheduler}
                        fallback={
                          <>
                            <div class="flex-1">
                              <select
                                value={item().pluginId}
                                onChange={(e) =>
                                  handlePluginChange(index, parseInt(e.currentTarget.value, 10))
                                }
                                class="w-full px-3 py-3 lg:py-2 text-base bg-paper border border-line rounded-lg focus:ring-2 focus:ring-brand focus:border-brand outline-none transition-all duration-200 disabled:border-0"
                              >
                                <For each={store.plugins}>
                                  {(plugin) => <option value={plugin.id}>{plugin.name}</option>}
                                </For>
                              </select>
                            </div>
                            <div class="flex items-center gap-3">
                              <div class="relative flex-1 lg:flex-initial">
                                <input
                                  type="number"
                                  min="1"
                                  max="86400"
                                  value={item().duration}
                                  onInput={(e) =>
                                    handleDurationChange(index, parseInt(e.currentTarget.value, 10))
                                  }
                                  class="w-full lg:w-32 pr-20 pl-3 py-3 lg:py-2 text-base bg-paper border border-line rounded-lg focus:ring-2 focus:ring-brand focus:border-brand outline-none transition-all duration-200 disabled:border-0"
                                />
                                <span class="absolute right-3 top-1/2 -translate-y-1/2 text-muted text-sm">
                                  {item().duration > 1 ? "seconds" : "second"}
                                </span>
                              </div>

                              <button
                                type="button"
                                onClick={() => handleRemoveItem(index)}
                                class="p-3 lg:p-2 text-muted hover:text-red-500 hover:bg-red-50 rounded-lg transition-all duration-200 min-w-[44px] min-h-[44px] lg:min-w-0 lg:min-h-0"
                                aria-label="Remove item"
                              >
                                <i class="fas fa-trash-alt text-lg" />
                              </button>
                            </div>
                          </>
                        }
                      >
                        <div class="w-full">
                          {store.plugins?.find((p) => p.id === item().pluginId)?.name ??
                            "Unknown Plugin"}
                        </div>
                        <div class="flex items-center gap-3">
                          <div class="relative">
                            {item().duration}
                            <span class="ml-2 text-sm text-muted">
                              {item().duration > 1 ? "seconds" : "second"}
                            </span>
                          </div>
                        </div>
                      </Show>
                    </div>
                  )}
                </Index>

                <div class="mt-4 flex flex-col sm:flex-row gap-2">
                  <Show when={store.schedule.length > 0}>
                    <button
                      type="button"
                      onClick={handleToggleScheduler}
                      class={`btn flex-1 ${
                        store.isActiveScheduler ? "bg-red-600 hover:bg-red-700" : ""
                      }`}
                    >
                      <i class={`fa-solid ${store.isActiveScheduler ? "fa-stop" : "fa-play"}`} />
                      <span>{store.isActiveScheduler ? "Stop Scheduler" : "Start Scheduler"}</span>
                    </button>
                  </Show>
                  <Show when={!store.isActiveScheduler}>
                    <button type="button" onClick={handleAddItem} class="btn flex-1">
                      <i class="fa-solid fa-plus" />
                      <span>Add Plugin</span>
                    </button>
                  </Show>
                </div>
              </Show>
            </div>
          </div>
        </div>
      }
      sidebar={
        <>
          <div class="space-y-6">
            <div class="space-y-3">
              <h3 class="section-title">Controls</h3>

              <div class="flex flex-col gap-2">
                <Button onClick={handleAddItem}>
                  <i class="fa-solid fa-plus mr-2" />
                  Add Plugin
                </Button>

                <Show when={store.schedule.length > 0}>
                  <div class="my-6 border-t border-line" />
                  <ToggleScheduleButton />
                </Show>

                <Show when={store.isActiveScheduler}>
                  <div class="my-6 border-t border-line" />
                  <ResetScheduleButton />
                </Show>
              </div>
            </div>
          </div>
          <div class="mt-auto pt-6 border-t border-line">
            <a href="#/" class="inline-flex items-center text-ink hover:text-brand font-medium">
              <i class="fa-solid fa-arrow-left mr-2" />
              Back to Main
            </a>
          </div>
        </>
      }
    />
  );
};

export default Scheduler;
