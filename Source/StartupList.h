/* Copyright 1996-2010 Arch D. Robison 

   Licensed under the Apache License, Version 2.0 (the "License"); 
   you may not use this file except in compliance with the License. 
   You may obtain a copy of the License at 
   
       http://www.apache.org/licenses/LICENSE-2.0 
       
   Unless required by applicable law or agreed to in writing, software 
   distributed under the License is distributed on an "AS IS" BASIS, 
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. 
   See the License for the specific language governing permissions and 
   limitations under the License.
 */

/******************************************************************************
 Support of collecting lists of items at program startup
*******************************************************************************/

template<typename T>
class StartupListItem {
    StartupListItem* next;
    static StartupListItem* root;
    static StartupListItem** end;
public:
    StartupListItem() {
        *end = this;
        end = &this->next;
    }

    //! Invoke "F(item) for each item in the list.
    template<typename F>
    static void forAll( F f ) {
        for( StartupListItem* p = root; p; p=p->next )
            f(*static_cast<T*>(p));
    }

    //! Invoke item.f() for each item in the list.
    static void forAll( void (T::*f)() ) {
         for( StartupListItem* p = root; p; p=p->next )
            (static_cast<T*>(p)->*f)();
   }
};

template<typename T>
StartupListItem<T>* StartupListItem<T>::root;

template<typename T>
StartupListItem<T>** StartupListItem<T>::end = &StartupListItem<T>::root;
