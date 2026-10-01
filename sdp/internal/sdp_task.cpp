#include "sdp/sdp_task.h"
#include "../sdp_module.h"

namespace bluetooth
{

sdp_task::sdp_task()
{
    set_target_module( sdp_module::s_sdp_module_name );
    m_task_type = static_cast<framework::task_type>( s_sdp_task_type_id );
}

}

