#include "ysort.h"

int ysort_render_params_partition(std::vector<RenderSpriteParams>& params, int low, int high) {
    RenderSpriteParams pivot = params[high];
    int i = low - 1;

    for (int j = low; j <= high - 1; j++) {
        if (params[j].ysort_position < pivot.ysort_position) {
            i++;
            RenderSpriteParams temp = params[j];
            params[j] = params[i];
            params[i] = temp;
        }
    }

    RenderSpriteParams temp = params[high];
    params[high] = params[i + 1];
    params[i + 1] = temp;

    return i + 1;
}

void ysort_render_params(std::vector<RenderSpriteParams>& params, int low, int high) {
    if (low < high) {
        int partition_index = ysort_render_params_partition(params, low, high);
        ysort_render_params(params, low, partition_index - 1);
        ysort_render_params(params, partition_index + 1, high);
    }
}
