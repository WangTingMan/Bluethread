/*
 * Bluethread - Self-developed dual-mode Bluetooth protocol stack
 * Copyright (C) 2026 Wang Fei.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License v3.0 for more details.
 *
 * Commercial closed-source licenses are available upon request.
 */

#pragma once
#include <cstdint>
#include <map>
#include <memory>
#include <vector>

namespace bluetooth
{

template<typename... Args>
std::vector<void*> make_params_pointer_vector( Args&&... args )
{
    std::vector<void*> ret;
    ( ret.push_back( reinterpret_cast<void*>( &std::forward<Args>( args ) ) ), ... );
    return ret;
}

class state_machine
{
public:

    enum { invalid_state = -1 };

    class abstract_event
    {

    public:

        virtual ~abstract_event(){}

    };

    /**
     * The state in the state machine.
     */
    class abstract_state
    {
        friend class state_machine;

    public:
        /**
         * Constructor.
         *
         * @param m_sm the abstract_state Machine to use
         * @param state_id the unique abstract_state ID. It should be a non-negative number.
         */
        abstract_state( state_machine& a_sm, uint32_t a_state_id )
            : m_sm( a_sm )
            , m_state_id( a_state_id ) {}

        virtual ~abstract_state() = default;

        /**
         * Process an event.
         * TODO: The arguments are wrong - used for backward compatibility.
         * Will be replaced later.
         *
         * @param event the event type
         * @param p_data the event data
         * @return true if the processing was completed, otherwise false
         */
        virtual bool handle_event( uint32_t event, void* p_data ) = 0;

        virtual bool handle_event( std::shared_ptr<abstract_event> const& a_event ) = 0;

        /**
         * Process an event.
         *
         * @param a_event_type the event type
         * @param a_params the event data
         * @return true if the processing was completed, otherwise false
         * warning: The caller must make sure that the pointer is valid in a_params.
         */
        virtual bool handle_event( uint32_t a_event_type, std::vector<void*> a_params )
        {
            return true;
        }

        /**
         * Get the abstract_state ID.
         *
         * @return the abstract_state ID
         */
        int get_id() const { return m_state_id; }

    protected:
        /**
         * Called when a state is entered.
         */
        virtual void on_enter() {}

        /**
         * Called when a state is exited.
         */
        virtual void on_exit() {}

        /**
         * Transition the abstract_state Machine to a new state.
         *
         * @param dest_state_id the state ID to transition to. It must be one
         * of the unique state IDs when the corresponding state was created.
         */
        void transition_to( uint32_t a_dest_state_id )
        {
            m_sm.transition_to( a_dest_state_id );
        }

        /**
         * Transition the abstract_state Machine to a new state.
         *
         * @param dest_state the state to transition to. It cannot be nullptr.
         */
        void transition_to( std::shared_ptr<abstract_state> const& a_dest_state )
        {
            m_sm.transition_to( a_dest_state );
        }

    private:
        state_machine& m_sm;
        uint32_t m_state_id;
    };

    state_machine()
    {
    }

    ~state_machine()
    {
    }

    /**
     * start the abstract_state Machine operation.
     */
    void start()
    {
        transition_to( m_initial_state );
    }

    /**
     * quit the abstract_state Machine operation.
     */
    void quit()
    {
        m_previous_state = m_current_state = nullptr;
    }

    /**
     * Get the current abstract_state ID.
     *
     * @return the current abstract_state ID
     */
    int get_id() const
    {
        if( m_current_state != nullptr )
        {
            return m_current_state->get_id();
        }
        return invalid_state;
    }

    int get_current_state_id() const
    {
        return get_id();
    }

    /**
     * Get the previous current abstract_state ID.
     *
     * @return the previous abstract_state ID
     */
    int previous_state_id() const
    {
        if( m_previous_state != nullptr )
        {
            return m_previous_state->get_id();
        }
        return invalid_state;
    }

    /**
     * Process an event.
     * TODO: The arguments are wrong - used for backward compatibility.
     * Will be replaced later.
     *
     * @param event the event type
     * @param p_data the event data
     * @return true if the processing was completed, otherwise false
     */
    bool handle_event( uint32_t a_event, void* a_data )
    {
        if( !m_current_state )
        {
            return false;
        }
        return m_current_state->handle_event( a_event, a_data );
    }

    bool handle_event( std::shared_ptr<abstract_event> const& a_event )
    {
        if( !m_current_state )
        {
            return false;
        }
        return m_current_state->handle_event( a_event );
    }

    bool handle_event( uint32_t a_event_type, std::vector<void*> a_params )
    {
        if( !m_current_state )
        {
            return false;
        }
        return m_current_state->handle_event( a_event_type, a_params );
    }

    template<typename... Args>
    bool handle_event( uint32_t a_event_type, Args&&... args )
    {
        return handle_event(a_event_type,
            make_params_pointer_vector( std::forward<Args>( args )... ) );
    }

    std::shared_ptr<abstract_state> find_state( uint32_t a_state_id )
    {
        auto it = m_states.find( a_state_id );
        if( it != m_states.end() )
        {
            return it->second;
        }

        return nullptr;
    }

    /**
     * Transition the abstract_state Machine to a new state.
     *
     * @param dest_state_id the state ID to transition to. It must be one
     * of the unique state IDs when the corresponding state was created.
     */
    void transition_to( uint32_t a_dest_state_id )
    {
        auto it = m_states.find( a_dest_state_id );

        transition_to( it->second );
    }

    /**
     * Transition the abstract_state Machine to a new state.
     *
     * @param dest_state the state to transition to. It cannot be nullptr.
     */
    void transition_to( std::shared_ptr<abstract_state> const& a_dest_state )
    {
        if( m_current_state != nullptr )
        {
            m_current_state->on_exit();
        }
        m_previous_state = m_current_state;
        m_current_state = a_dest_state;
        m_current_state->on_enter();
    }

    /**
     * Add a state to the abstract_state Machine.
     * The state machine takes ownership on the state - i.e., the state will
     * be deleted by the abstract_state Machine itself.
     *
     * @param state the state to add
     */
    void add_state( std::shared_ptr<abstract_state> a_state )
    {
        m_states.insert( std::make_pair( a_state->get_id(),  std::move( a_state ) ) );
    }

    /**
     * Set the initial state of the abstract_state Machine.
     *
     * @param initial_state the initial state
     */
    void set_initial_state( std::shared_ptr<abstract_state> a_initial_state )
    {
        m_initial_state = std::move( a_initial_state );
    }

private:

    std::shared_ptr<abstract_state> m_initial_state;
    std::shared_ptr<abstract_state> m_previous_state;
    std::shared_ptr<abstract_state> m_current_state;
    std::map<uint32_t, std::shared_ptr<abstract_state>> m_states;
};

}
