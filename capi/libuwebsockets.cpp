#include "libuwebsockets.h"
#include <string_view>
#include <malloc.h>
#include "App.h"

extern "C"
{

    void uws_timer_close(uws_timer_t* timer){
        struct us_timer_t *t = (struct us_timer_t *) timer;
        struct timer_handler_data *data;
        memcpy(&data, us_timer_ext(t), sizeof(struct timer_handler_data *));
        free(data);    
        us_timer_close(t);
    }

    uws_timer_t* uws_create_timer(int ms, int repeat_ms, void (*handler)(void *data), void *data)
    {
        struct us_loop_t *loop = (struct us_loop_t *)uWS::Loop::get();
        struct us_timer_t *delayTimer = us_create_timer(loop, 0, sizeof(void *));

        struct timer_handler_data
        {
            void *data;
            void (*handler)(void *data);
            bool repeat;
        };

        struct timer_handler_data *timer_data = (struct timer_handler_data *)malloc(sizeof(timer_handler_data));
        timer_data->data = data;
        timer_data->handler = handler;
        timer_data->repeat = repeat_ms > 0;
        memcpy(us_timer_ext(delayTimer), &timer_data, sizeof(struct timer_handler_data *));

        us_timer_set(
            delayTimer, [](struct us_timer_t *t)
            {
                /* We wrote the pointer to the timer's extension */
                struct timer_handler_data *data;
                memcpy(&data, us_timer_ext(t), sizeof(struct timer_handler_data *));

                data->handler(data->data);
                
                if(!data->repeat){
                    free(data);    
                    us_timer_close(t);
                }
            },
            ms, repeat_ms);

        return (uws_timer_t*)delayTimer;
    }
}